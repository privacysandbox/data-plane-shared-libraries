// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/capability.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/core/common/uuid/uuid.h"
#include "src/roma/byob/dispatcher/dispatcher.h"
#include "src/roma/byob/dispatcher/dispatcher.pb.h"
#include "src/util/status_macro/status_macros.h"

ABSL_FLAG(std::string, socket_name, "/sockdir/abcd.sock",
          "Server socket for reaching Roma app API");
ABSL_FLAG(std::vector<std::string>, mounts,
          std::vector<std::string>({"/lib", "/lib64"}),
          "Mounts containing dependencies needed by the binary");
ABSL_FLAG(std::string, log_dir, "/log_dir", "Directory used for storing logs");

namespace {
using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using ::privacy_sandbox::server_common::byob::DispatcherRequest;
using ::privacy_sandbox::server_common::byob::kNumTokenBytes;

bool ConnectToPath(int fd, std::string_view socket_name) {
  ::sockaddr_un sa = {
      .sun_family = AF_UNIX,
  };
  socket_name.copy(sa.sun_path, sizeof(sa.sun_path));
  return ::connect(fd, reinterpret_cast<::sockaddr*>(&sa), SUN_LEN(&sa)) == 0;
}
struct WorkerImplArg {
  absl::Span<const std::string> mounts;
  std::string_view execution_token;
  std::string_view pivot_root_dir;
  int rpc_fd;
  std::string_view code_token;
  std::string_view binary_path;
  int dev_null_fd;
  bool enable_log_egress;
  std::string_view log_dir_name;
};

absl::Status SetPrctlOptions(
    absl::Span<const std::pair<int, int>> option_arg_pairs) {
  for (const auto& [option, arg] : option_arg_pairs) {
    if (::prctl(option, arg) < 0) {
      return absl::ErrnoToStatus(
          errno, absl::StrCat("Failed prctl(", option, ", ", arg, ")"));
    }
  }
  return absl::OkStatus();
}

absl::Status CreateDirectories(const std::filesystem::path& path) {
  if (std::error_code ec; !std::filesystem::create_directories(path, ec)) {
    return absl::InternalError(
        absl::StrCat("Failed to create '", path.native(), "': ", ec.message()));
  }
  return absl::OkStatus();
}

absl::Status Mount(const char* source, const char* target,
                   const char* filesystemtype, int mountflags) {
  if (::mount(source, target, filesystemtype, mountflags, nullptr) == -1) {
    return absl::ErrnoToStatus(errno, absl::StrCat("Failed to mount '", source,
                                                   "' to '", target, "'"));
  }
  return absl::OkStatus();
}

absl::Status Dup2(int oldfd, int newfd) {
  if (::dup2(oldfd, newfd) == -1) {
    return absl::ErrnoToStatus(errno,
                               absl::StrCat("dup2(", oldfd, ",", newfd, ")"));
  }
  return absl::OkStatus();
}

absl::Status SetupSandbox(const WorkerImplArg& worker_impl_arg) {
  int log_fd = -1;
  if (worker_impl_arg.enable_log_egress) {
    const std::filesystem::path log_file_name =
        std::filesystem::path(worker_impl_arg.log_dir_name) /
        absl::StrCat(worker_impl_arg.execution_token, ".log");
    log_fd = ::open(log_file_name.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (log_fd < 0) {
      return absl::ErrnoToStatus(
          errno, absl::StrCat("Failed to open '", log_file_name.native(), "'"));
    }
  }

  // Set up restricted filesystem for worker using pivot_root
  // pivot_root doesn't work under an MS_SHARED mount point.
  // https://man7.org/linux/man-pages/man2/pivot_root.2.html.
  PS_RETURN_IF_ERROR(Mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE));
  for (const std::string& mount : worker_impl_arg.mounts) {
    const std::string target =
        absl::StrCat(worker_impl_arg.pivot_root_dir, mount);
    PS_RETURN_IF_ERROR(CreateDirectories(target));
    PS_RETURN_IF_ERROR(Mount(mount.c_str(), target.c_str(), nullptr, MS_BIND));
  }
  const std::string binary_dir =
      std::filesystem::path(worker_impl_arg.binary_path).parent_path();
  {
    const std::string target =
        absl::StrCat(worker_impl_arg.pivot_root_dir, binary_dir);
    PS_RETURN_IF_ERROR(CreateDirectories(target));
    PS_RETURN_IF_ERROR(
        Mount(binary_dir.c_str(), target.c_str(), nullptr, MS_BIND));
  }

  // MS_REC needed here to get other mounts (/lib, /lib64 etc)
  PS_RETURN_IF_ERROR(Mount(worker_impl_arg.pivot_root_dir.data(),
                           worker_impl_arg.pivot_root_dir.data(), "bind",
                           MS_REC | MS_BIND));
  PS_RETURN_IF_ERROR(Mount(worker_impl_arg.pivot_root_dir.data(),
                           worker_impl_arg.pivot_root_dir.data(), "bind",
                           MS_REC | MS_SLAVE));
  {
    const std::string pivot_dir =
        absl::StrCat(worker_impl_arg.pivot_root_dir, "/pivot");
    PS_RETURN_IF_ERROR(CreateDirectories(pivot_dir));
    if (::syscall(SYS_pivot_root, worker_impl_arg.pivot_root_dir.data(),
                  pivot_dir.c_str()) == -1) {
      return absl::ErrnoToStatus(
          errno, absl::StrCat("syscall(SYS_pivot_root, '",
                              worker_impl_arg.pivot_root_dir, "', '", pivot_dir,
                              "')"));
    }
  }
  if (::chdir("/") == -1) {
    return absl::ErrnoToStatus(errno, "chdir('/')");
  }
  if (::umount2("/pivot", MNT_DETACH) == -1) {
    return absl::ErrnoToStatus(errno, "mount2('/pivot', MNT_DETACH)");
  }
  if (::rmdir("/pivot") == -1) {
    return absl::ErrnoToStatus(errno, "rmdir('/pivot')");
  }
  for (const std::string& mount : worker_impl_arg.mounts) {
    PS_RETURN_IF_ERROR(
        Mount(mount.c_str(), mount.c_str(), nullptr, MS_REMOUNT | MS_BIND));
  }
  PS_RETURN_IF_ERROR(Mount(binary_dir.c_str(), binary_dir.c_str(), nullptr,
                           MS_REMOUNT | MS_BIND));
  PS_RETURN_IF_ERROR(SetPrctlOptions({
      {PR_CAPBSET_DROP, CAP_SYS_ADMIN},
      {PR_CAPBSET_DROP, CAP_SETPCAP},
      {PR_SET_PDEATHSIG, SIGHUP},
  }));
  PS_RETURN_IF_ERROR(Dup2(worker_impl_arg.dev_null_fd, STDOUT_FILENO));
  if (!worker_impl_arg.enable_log_egress) {
    PS_RETURN_IF_ERROR(Dup2(worker_impl_arg.dev_null_fd, STDERR_FILENO));
  } else {
    PS_RETURN_IF_ERROR(Dup2(log_fd, STDERR_FILENO));
  }
  return absl::OkStatus();
}

constexpr uint32_t MaxIntDecimalLength() {
  int n = std::numeric_limits<int>::max();
  uint32_t count = 0;
  while (n > 0) {
    n /= 10;
    count++;
  }
  return count;
}

int WorkerImpl(void* arg) {
  const WorkerImplArg& worker_impl_arg = *static_cast<WorkerImplArg*>(arg);
  PCHECK(::write(worker_impl_arg.rpc_fd, worker_impl_arg.code_token.data(),
                 kNumTokenBytes) == kNumTokenBytes);
  PCHECK(::write(worker_impl_arg.rpc_fd, worker_impl_arg.execution_token.data(),
                 kNumTokenBytes) == kNumTokenBytes);

  // Add one to decimal length because `snprintf` adds a null terminator.
  char connection_fd[MaxIntDecimalLength() + 1];
  const int fd = ::dup(worker_impl_arg.rpc_fd);
  PCHECK(fd != -1);
  PCHECK(::snprintf(connection_fd, sizeof(connection_fd), "%d", fd) > 0);

  // Destructors will not run after `exec`. All objects must be destroyed and
  // all heap allocations must be freed prior to `exec`.
  CHECK_OK(SetupSandbox(worker_impl_arg));

  // Exec binary.
  ::execl(worker_impl_arg.binary_path.data(),
          worker_impl_arg.binary_path.data(), connection_fd, nullptr);
  PLOG(FATAL) << "exec '" << worker_impl_arg.binary_path << "' failed";
}
struct PidExecutionTokenAndPivotRootDir {
  int pid;
  std::string execution_token;
  std::string pivot_root_dir;
};

// Returns `std::nullopt` when workers can no longer be created: in virtually
// all cases, this is because Roma is shutting down and is not an error.
std::optional<PidExecutionTokenAndPivotRootDir> ConnectSendCloneAndExec(
    absl::Span<const std::string> mounts, std::string_view socket_name,
    std::string_view code_token, std::string_view binary_path,
    const int dev_null_fd, std::string_view log_dir_name,
    bool enable_log_egress = false) {
  const int rpc_fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (rpc_fd == -1) {
    PLOG(ERROR) << "socket()";
    return std::nullopt;
  }
  absl::Cleanup cleanup = [rpc_fd] {
    if (::close(rpc_fd) == -1) {
      PLOG(ERROR) << "close()";
    }
  };
  if (!ConnectToPath(rpc_fd, socket_name)) {
    PLOG(INFO) << "connect() to " << socket_name << " failed";
    return std::nullopt;
  }
  std::string pivot_root_dir = "/tmp/roma_app_server_XXXXXX";
  if (::mkdtemp(pivot_root_dir.data()) == nullptr) {
    PLOG(ERROR) << "mkdtemp()";
    return std::nullopt;
  }
  std::string execution_token =
      ToString(google::scp::core::common::Uuid::GenerateUuid());
  WorkerImplArg worker_impl_arg{
      .mounts = mounts,
      .execution_token = execution_token,
      .pivot_root_dir = pivot_root_dir,
      .rpc_fd = rpc_fd,
      .code_token = code_token,
      .binary_path = binary_path,
      .dev_null_fd = dev_null_fd,
      .enable_log_egress = enable_log_egress,
      .log_dir_name = log_dir_name,
  };

  // Explicitly 16-byte align the stack. Otherwise, `clone` on aarch64 may hang
  // or the process may receive SIGBUS (depending on the size of the stack
  // before this function call). Overprovisions stack by at most 15 bytes (of
  // 2^10 bytes) where unneeded.
  // https://community.arm.com/arm-community-blogs/b/architectures-and-processors-blog/posts/using-the-stack-in-aarch32-and-aarch64
  alignas(16) char stack[1 << 20];
  const pid_t pid =
      ::clone(WorkerImpl, stack + sizeof(stack),
              CLONE_VM | CLONE_VFORK | CLONE_NEWIPC | CLONE_NEWPID | SIGCHLD |
                  CLONE_NEWUTS | CLONE_NEWNS,
              &worker_impl_arg);
  if (pid == -1) {
    PLOG(ERROR) << "clone()";
    if (std::error_code ec; !std::filesystem::remove(pivot_root_dir, ec)) {
      LOG(ERROR) << "Failed to remove " << pivot_root_dir << ": " << ec;
    }
    return std::nullopt;
  }
  return PidExecutionTokenAndPivotRootDir{
      .pid = pid,
      .execution_token = std::move(execution_token),
      .pivot_root_dir = std::move(pivot_root_dir),
  };
}
}  // namespace

int main(int argc, char** argv) {
  std::vector<char*> args = absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  LOG(INFO) << "Starting up.";
  const std::string socket_name = absl::GetFlag(FLAGS_socket_name);
  const std::vector<std::string> mounts = absl::GetFlag(FLAGS_mounts);
  const std::string log_dir_name = absl::GetFlag(FLAGS_log_dir);
  const std::filesystem::path progdir =
      std::filesystem::temp_directory_path() /
      ToString(google::scp::core::common::Uuid::GenerateUuid());
  if (std::error_code ec; !std::filesystem::create_directories(progdir, ec)) {
    LOG(ERROR) << "Failed to create " << progdir << ": " << ec;
    return -1;
  }
  absl::Cleanup progdir_cleanup = [&progdir] {
    if (std::error_code ec; !std::filesystem::remove_all(progdir, ec)) {
      LOG(ERROR) << "Failed to remove " << progdir << ": " << ec;
    }
  };
  const int rpc_fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (rpc_fd == -1) {
    PLOG(ERROR) << "socket()";
    return -1;
  }
  if (!ConnectToPath(rpc_fd, socket_name)) {
    PLOG(ERROR) << "connect() to " << socket_name << " failed";
    return -1;
  }
  const int dev_null_fd = ::open("/dev/null", O_WRONLY);
  if (dev_null_fd < 0) {
    PLOG(ERROR) << "open failed for /dev/null";
    return -1;
  }
  struct UdfInstanceMetadata {
    std::string pivot_root_dir;
    std::string execution_token;
    std::string code_token;
    std::string binary_path;
    bool enable_log_egress;
    bool marked_for_deletion = false;
  };

  absl::Mutex mu;
  absl::flat_hash_map<int, UdfInstanceMetadata> pid_to_udf;  // Guarded by mu.
  bool shutdown = false;                                     // Guarded by mu.
  std::thread reloader([&socket_name, &mounts, &mu, &pid_to_udf, &shutdown,
                        &dev_null_fd, &log_dir_name] {
    while (true) {
      {
        auto fn = [&] {
          mu.AssertReaderHeld();
          return !pid_to_udf.empty() || shutdown;
        };
        absl::MutexLock lock(&mu);

        // Wait until at least one worker has been created before reloading.
        mu.Await(absl::Condition(&fn));
        if (shutdown) {
          return;
        }
      }
      while (true) {
        UdfInstanceMetadata udf;
        int status;

        // Wait for any worker to end.
        const int pid = ::waitpid(-1, &status, /*options=*/0);
        if (pid == -1) {
          PLOG(INFO) << "waitpid()";
          break;
        }
        absl::MutexLock lock(&mu);
        if (shutdown) {
          return;
        }
        const auto it = pid_to_udf.find(pid);
        if (it == pid_to_udf.end()) {
          LOG(ERROR) << "waitpid() returned unknown pid=" << pid;
          continue;
        }
        udf = std::move(it->second);
        pid_to_udf.erase(it);
        if (!WIFEXITED(status)) {
          if (WIFSIGNALED(status)) {
            LOG(INFO) << "Process pid=" << pid
                      << " terminated (signal=" << WTERMSIG(status)
                      << ", coredump=" << WCOREDUMP(status) << ")";
          } else {
            LOG(INFO) << "Process pid=" << pid << " did not exit";
          }
        } else if (const int exit_code = WEXITSTATUS(status); exit_code != 0) {
          LOG(INFO) << "Process pid=" << pid << " exit_code=" << exit_code;
        }
        CHECK(std::filesystem::remove_all(udf.pivot_root_dir));
        if (udf.marked_for_deletion) {
          std::filesystem::remove_all(
              std::filesystem::path(udf.binary_path).parent_path());
          continue;
        }

        // Start a new worker with the same UDF as the most-recently ended
        // worker.
        std::optional<PidExecutionTokenAndPivotRootDir>
            pid_execution_token_and_pivot_root_dir = ConnectSendCloneAndExec(
                mounts, socket_name, udf.code_token, udf.binary_path,
                dev_null_fd, log_dir_name, udf.enable_log_egress);
        if (!pid_execution_token_and_pivot_root_dir.has_value()) {
          return;
        }
        udf.pivot_root_dir =
            std::move(pid_execution_token_and_pivot_root_dir->pivot_root_dir);
        udf.execution_token =
            std::move(pid_execution_token_and_pivot_root_dir->execution_token);
        CHECK(pid_to_udf
                  .try_emplace(pid_execution_token_and_pivot_root_dir->pid,
                               std::move(udf))
                  .second);
      }
    }
  });
  absl::Cleanup cleanup = [&] {
    LOG(INFO) << "Shutting down.";
    {
      absl::MutexLock lock(&mu);
      shutdown = true;

      // Kill extant workers before exit.
      for (const auto& [pid, udf] : pid_to_udf) {
        if (::kill(pid, SIGKILL) == -1) {
          // If the process has already terminated, degrade error to a log.
          if (errno == ESRCH) {
            PLOG(INFO) << "kill(" << pid << ", SIGKILL)";
          } else {
            PLOG(ERROR) << "kill(" << pid << ", SIGKILL)";
          }
        }
        if (::waitpid(pid, /*status=*/nullptr, /*options=*/0) == -1) {
          // If the child has already been reaped (likely by reloader thread),
          // degrade error to a log.
          if (errno == ECHILD) {
            PLOG(INFO) << "waitpid(" << pid << ", nullptr, 0)";
          } else {
            PLOG(ERROR) << "waitpid(" << pid << ", nullptr, 0)";
          }
        }
        if (std::error_code ec;
            !std::filesystem::remove_all(udf.pivot_root_dir, ec)) {
          LOG(ERROR) << "Failed to remove " << udf.pivot_root_dir << ": " << ec;
        }
        std::filesystem::remove_all(
            std::filesystem::path(udf.binary_path).parent_path());
      }
    }
    reloader.join();
    if (::close(dev_null_fd) == -1) {
      PLOG(ERROR) << "close(" << dev_null_fd << ")";
    }
  };
  FileInputStream input(rpc_fd);
  while (true) {
    DispatcherRequest request;
    if (!ParseDelimitedFromZeroCopyStream(&request, &input, nullptr)) {
      break;
    }
    if (request.request_case() == DispatcherRequest::REQUEST_NOT_SET) {
      LOG(ERROR) << "DispatcherRequest not set.";
      continue;
    }
    if (request.has_delete_binary()) {
      // Kill all workers and mark for removal.
      absl::MutexLock lock(&mu);
      for (auto& [pid, udf] : pid_to_udf) {
        if (request.delete_binary().code_token() == udf.code_token) {
          if (::kill(pid, SIGKILL) == -1) {
            PLOG(INFO) << "kill(" << pid << ", SIGKILL)";
          }
          udf.marked_for_deletion = true;
        }
      }
      continue;
    }
    if (request.has_cancel()) {
      // Kill the worker.
      absl::MutexLock lock(&mu);
      for (const auto& [pid, udf] : pid_to_udf) {
        if (request.cancel().execution_token() == udf.execution_token) {
          if (::kill(pid, SIGKILL) == -1) {
            PLOG(INFO) << "kill(" << pid << ", SIGKILL)";
          }
          break;
        }
      }
      continue;
    }
    if (!request.has_load_binary()) {
      LOG(ERROR) << "Unrecognized request from dispatcher.";
      continue;
    }

    // Load new binary.
    const std::filesystem::path binary_dir =
        progdir / request.load_binary().code_token();
    if (std::error_code ec;
        !std::filesystem::create_directory(binary_dir, ec)) {
      LOG(ERROR) << "Failed to create " << binary_dir << ": " << ec;
      return -1;
    }
    const std::filesystem::path binary_path =
        binary_dir / request.load_binary().code_token();
    if (request.load_binary().has_binary_content()) {
      const int fd =
          ::open(binary_path.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0500);
      if (fd == -1) {
        PLOG(ERROR) << "open()";
        return -1;
      }
      if (::write(fd, request.load_binary().binary_content().c_str(),
                  request.load_binary().binary_content().size()) !=
          request.load_binary().binary_content().size()) {
        PLOG(ERROR) << "write()";
        return -1;
      }

      // Flush the file to ensure the executable is closed for writing before
      // the `exec` call.
      if (::fsync(fd) == -1) {
        PLOG(ERROR) << "fsync()";
        return -1;
      }
      if (::close(fd) == -1) {
        PLOG(ERROR) << "close()";
        return -1;
      }
    } else if (request.load_binary().has_source_bin_code_token()) {
      std::filesystem::path existing_binary_path =
          progdir / request.load_binary().source_bin_code_token() /
          request.load_binary().source_bin_code_token();
      if (!std::filesystem::exists(existing_binary_path)) {
        LOG(ERROR) << "Expected binary " << existing_binary_path.c_str()
                   << " not found";
        return -1;
      } else if (!std::filesystem::is_regular_file(existing_binary_path)) {
        LOG(ERROR) << "File " << existing_binary_path.c_str()
                   << " not a regular file";
        return -1;
      }
      std::filesystem::create_hard_link(existing_binary_path, binary_path);
    } else {
      LOG(ERROR) << "Failed to load binary";
      return -1;
    }
    for (int i = 0; i < request.load_binary().num_workers() - 1; ++i) {
      // To ensure that the worker metadata exists in the pid_to_udf map when
      // reloader thread looks for it (post waitpid), acquire lock before
      // creating the worker and release once the worker metadata is populated
      // in pid_to_udf map.
      // TODO: b/375622989 - Refactor run_workers to make the code more
      // coherent.
      absl::MutexLock lock(&mu);
      std::optional<PidExecutionTokenAndPivotRootDir>
          pid_execution_token_and_pivot_root_dir = ConnectSendCloneAndExec(
              mounts, socket_name, request.load_binary().code_token(),
              binary_path.native(), dev_null_fd, log_dir_name,
              request.load_binary().enable_log_egress());
      if (!pid_execution_token_and_pivot_root_dir.has_value()) {
        return -1;
      }
      UdfInstanceMetadata udf{
          .pivot_root_dir =
              std::move(pid_execution_token_and_pivot_root_dir->pivot_root_dir),
          .execution_token = std::move(
              pid_execution_token_and_pivot_root_dir->execution_token),
          .code_token = request.load_binary().code_token(),
          .binary_path = binary_path,
          .enable_log_egress = request.load_binary().enable_log_egress(),
      };
      pid_to_udf[pid_execution_token_and_pivot_root_dir->pid] = std::move(udf);
    }

    // Start n-th worker out of loop.
    // To ensure that the worker metadata exists in the pid_to_udf map when
    // reloader thread looks for it (post waitpid), acquire lock before creating
    // the worker and release once the worker metadata is populated in
    // pid_to_udf map.
    // TODO: b/375622989 - Refactor run_workers to make the code more
    // coherent.
    absl::MutexLock lock(&mu);
    std::optional<PidExecutionTokenAndPivotRootDir>
        pid_execution_token_and_pivot_root_dir = ConnectSendCloneAndExec(
            mounts, socket_name, request.load_binary().code_token(),
            binary_path.native(), dev_null_fd, log_dir_name,
            request.load_binary().enable_log_egress());
    if (!pid_execution_token_and_pivot_root_dir.has_value()) {
      return -1;
    }
    UdfInstanceMetadata udf{
        .pivot_root_dir =
            std::move(pid_execution_token_and_pivot_root_dir->pivot_root_dir),
        .execution_token =
            std::move(pid_execution_token_and_pivot_root_dir->execution_token),
        .code_token =
            std::move(*request.mutable_load_binary()->mutable_code_token()),
        .binary_path = binary_path,
        .enable_log_egress = request.load_binary().enable_log_egress(),
    };
    pid_to_udf[pid_execution_token_and_pivot_root_dir->pid] = std::move(udf);
  }
  return 0;
}
