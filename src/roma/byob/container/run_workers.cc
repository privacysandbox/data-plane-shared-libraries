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

#include "absl/base/thread_annotations.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
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

absl::Status ConnectToPath(const int fd, std::string_view socket_name) {
  ::sockaddr_un sa = {
      .sun_family = AF_UNIX,
  };
  socket_name.copy(sa.sun_path, sizeof(sa.sun_path));
  if (::connect(fd, reinterpret_cast<::sockaddr*>(&sa), SUN_LEN(&sa)) == -1) {
    return absl::ErrnoToStatus(errno, "connect()");
  }
  return absl::OkStatus();
}
struct WorkerImplArg {
  absl::Span<const std::string> mounts;
  std::string_view execution_token;
  std::string_view pivot_root_dir;
  std::string_view socket_name;
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
  const int rpc_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  PCHECK(rpc_fd != -1);
  if (const absl::Status status =
          ConnectToPath(rpc_fd, worker_impl_arg.socket_name);
      !status.ok()) {
    LOG(INFO) << status;
    return -1;
  }
  PCHECK(::write(rpc_fd, worker_impl_arg.code_token.data(), kNumTokenBytes) ==
         kNumTokenBytes);
  PCHECK(::write(rpc_fd, worker_impl_arg.execution_token.data(),
                 kNumTokenBytes) == kNumTokenBytes);

  // Add one to decimal length because `snprintf` adds a null terminator.
  char connection_fd[MaxIntDecimalLength() + 1];
  PCHECK(::snprintf(connection_fd, sizeof(connection_fd), "%d", rpc_fd) > 0);

  // Destructors will not run after `exec`. All objects must be destroyed and
  // all heap allocations must be freed prior to `exec`.
  CHECK_OK(SetupSandbox(worker_impl_arg));

  // Exec binary.
  ::execl(worker_impl_arg.binary_path.data(),
          worker_impl_arg.binary_path.data(), connection_fd, nullptr);
  PLOG(FATAL) << "exec '" << worker_impl_arg.binary_path << "' failed";
}

class WorkerRunner final {
 public:
  WorkerRunner(std::string socket_name, std::vector<std::string> mounts,
               std::string log_dir_name, const int dev_null_fd,
               const std::filesystem::path& progdir)
      : socket_name_(std::move(socket_name)),
        mounts_(std::move(mounts)),
        log_dir_name_(std::move(log_dir_name)),
        dev_null_fd_(dev_null_fd),
        progdir_(progdir) {}

  ~WorkerRunner() {
    LOG(INFO) << "Shutting down.";
    absl::MutexLock lock(&mu_);
    active_code_token_to_pids_.clear();

    // Kill extant workers before exit.
    for (const auto& [pid, _] : pid_to_execution_token_) {
      if (::kill(pid, SIGKILL) == -1) {
        // If the process has already terminated, degrade error to a log.
        if (errno == ESRCH) {
          PLOG(INFO) << "kill(" << pid << ", SIGKILL)";
        } else {
          PLOG(ERROR) << "kill(" << pid << ", SIGKILL)";
        }
      }
    }
    auto fn = [&] {
      mu_.AssertReaderHeld();
      return code_token_to_thread_count_.empty();
    };
    mu_.Await(absl::Condition(&fn));
  }

  absl::Status Run() ABSL_LOCKS_EXCLUDED(mu_) {
    const int rpc_fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (rpc_fd == -1) {
      return absl::ErrnoToStatus(errno, "socket()");
    }
    absl::Cleanup rpc_fd_cleanup = [rpc_fd] { ::close(rpc_fd); };
    PS_RETURN_IF_ERROR(ConnectToPath(rpc_fd, socket_name_));
    FileInputStream input(rpc_fd);
    while (true) {
      DispatcherRequest request;
      if (!ParseDelimitedFromZeroCopyStream(&request, &input, nullptr)) {
        // The socket has been shutdown by the dispatcher.
        return absl::OkStatus();
      }
      if (request.request_case() == DispatcherRequest::REQUEST_NOT_SET) {
        LOG(ERROR) << "DispatcherRequest not set.";
        continue;
      }
      if (request.has_delete_binary()) {
        Delete(std::move(*request.mutable_delete_binary()));
        continue;
      }
      if (request.has_cancel()) {
        Cancel(std::move(*request.mutable_cancel()));
        continue;
      }
      if (!request.has_load_binary()) {
        LOG(ERROR) << "Unrecognized request from dispatcher.";
        continue;
      }
      const std::filesystem::path binary_dir =
          progdir_ / request.load_binary().code_token();
      if (std::error_code ec;
          !std::filesystem::create_directory(binary_dir, ec)) {
        return absl::InternalError(absl::StrCat(
            "Failed to create ", binary_dir.native(), ": ", ec.message()));
      }
      std::filesystem::path binary_path =
          binary_dir / request.load_binary().code_token();
      if (request.load_binary().has_binary_content()) {
        PS_RETURN_IF_ERROR(
            SaveNewBinary(binary_path, request.load_binary().binary_content()));
      } else if (request.load_binary().has_source_bin_code_token()) {
        PS_RETURN_IF_ERROR(CreateHardLinkForExistingBinary(
            binary_path, request.load_binary().source_bin_code_token()));
      } else {
        return absl::InternalError("Failed to load binary");
      }
      PS_RETURN_IF_ERROR(CreateWorkerPool(
          std::move(binary_path),
          std::move(*request.mutable_load_binary()->mutable_code_token()),
          request.load_binary().num_workers(),
          request.load_binary().enable_log_egress()));
    }
  }

 private:
  struct PidExecutionTokenAndPivotRootDir {
    int pid;
    std::string execution_token;
    std::string pivot_root_dir;
  };

  absl::StatusOr<PidExecutionTokenAndPivotRootDir> ConnectSendCloneAndExec(
      std::string_view code_token, std::string_view binary_path,
      const bool enable_log_egress) {
    std::string pivot_root_dir = "/tmp/roma_app_server_XXXXXX";
    if (::mkdtemp(pivot_root_dir.data()) == nullptr) {
      return absl::ErrnoToStatus(errno, "mkdtemp()");
    }
    std::string execution_token =
        ToString(google::scp::core::common::Uuid::GenerateUuid());
    WorkerImplArg worker_impl_arg{
        .mounts = mounts_,
        .execution_token = execution_token,
        .pivot_root_dir = pivot_root_dir,
        .socket_name = socket_name_,
        .code_token = code_token,
        .binary_path = binary_path,
        .dev_null_fd = dev_null_fd_,
        .enable_log_egress = enable_log_egress,
        .log_dir_name = log_dir_name_,
    };

    // Explicitly 16-byte align the stack. Otherwise, `clone` on aarch64 may
    // hang or the process may receive SIGBUS (depending on the size of the
    // stack before this function call). Overprovisions stack by at most 15
    // bytes (of 2^10 bytes) where unneeded.
    // https://community.arm.com/arm-community-blogs/b/architectures-and-processors-blog/posts/using-the-stack-in-aarch32-and-aarch64
    alignas(16) char stack[1 << 20];
    const pid_t pid =
        ::clone(WorkerImpl, stack + sizeof(stack),
                CLONE_VM | CLONE_VFORK | CLONE_NEWIPC | CLONE_NEWPID | SIGCHLD |
                    CLONE_NEWUTS | CLONE_NEWNS,
                &worker_impl_arg);
    if (pid == -1) {
      if (std::error_code ec; !std::filesystem::remove(pivot_root_dir, ec)) {
        LOG(ERROR) << "Failed to remove " << pivot_root_dir << ": " << ec;
      }
      return absl::ErrnoToStatus(errno, "clone()");
    }
    return PidExecutionTokenAndPivotRootDir{
        .pid = pid,
        .execution_token = std::move(execution_token),
        .pivot_root_dir = std::move(pivot_root_dir),
    };
  }

  void ReloaderImpl(int pid, std::string pivot_root_dir,
                    const std::string code_token, const std::string binary_path,
                    const bool enable_log_egress) ABSL_LOCKS_EXCLUDED(mu_) {
    while (true) {
      int status;
      if (::waitpid(pid, &status, /*options=*/0) == -1) {
        PLOG(INFO) << "waitpid()";
      }
      {
        absl::MutexLock lock(&mu_);
        pid_to_execution_token_.erase(pid);
        if (const auto it = active_code_token_to_pids_.find(code_token);
            it == active_code_token_to_pids_.end()) {
          // Code token is no longer active. Initiate binary cleanup.
          break;
        } else {
          it->second.erase(pid);
        }
      }
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

      // Start a new worker.
      absl::StatusOr<PidExecutionTokenAndPivotRootDir>
          pid_execution_token_and_pivot_root_dir = ConnectSendCloneAndExec(
              code_token, binary_path, enable_log_egress);
      if (!pid_execution_token_and_pivot_root_dir.ok()) {
        break;
      }
      pid = pid_execution_token_and_pivot_root_dir->pid;
      if (std::error_code ec; std::filesystem::remove_all(pivot_root_dir, ec) ==
                              static_cast<std::uintmax_t>(-1)) {
        LOG(ERROR) << "Failed to remove " << pivot_root_dir << ": " << ec;
      }
      pivot_root_dir =
          std::move(pid_execution_token_and_pivot_root_dir->pivot_root_dir);
      {
        absl::MutexLock lock(&mu_);
        pid_to_execution_token_[pid] =
            std::move(pid_execution_token_and_pivot_root_dir->execution_token);
        if (const auto it = active_code_token_to_pids_.find(code_token);
            it == active_code_token_to_pids_.end()) {
          // Code token was deleted. Cleanup on next iteration.
          if (::kill(pid, SIGKILL) == -1) {
            PLOG(INFO) << "kill(" << pid << ", SIGKILL)";
          }
        } else {
          it->second.insert(pid);
        }
      }
    }
    if (std::error_code ec; std::filesystem::remove_all(pivot_root_dir, ec) ==
                            static_cast<std::uintmax_t>(-1)) {
      LOG(ERROR) << "Failed to remove " << pivot_root_dir << ": " << ec;
    }
    const std::filesystem::path binary_dir =
        std::filesystem::path(binary_path).parent_path();
    absl::MutexLock lock(&mu_);
    const auto it = code_token_to_thread_count_.find(code_token);
    CHECK(it != code_token_to_thread_count_.end());

    // Delete binary if this is the last worker for the code token.
    if (--it->second == 0) {
      if (std::error_code ec; std::filesystem::remove_all(binary_dir, ec) ==
                              static_cast<std::uintmax_t>(-1)) {
        LOG(INFO) << "Failed to remove " << binary_dir.native() << ": " << ec;
      }
      code_token_to_thread_count_.erase(it);
    }
  }

  void Delete(DispatcherRequest::DeleteBinary request)
      ABSL_LOCKS_EXCLUDED(mu_) {
    // Kill all workers and mark for removal.
    absl::MutexLock lock(&mu_);
    if (const auto it = active_code_token_to_pids_.find(request.code_token());
        it != active_code_token_to_pids_.end()) {
      for (const int pid : it->second) {
        if (::kill(pid, SIGKILL) == -1) {
          PLOG(INFO) << "kill(" << pid << ", SIGKILL)";
        }
      }
      active_code_token_to_pids_.erase(it);
    }
  }

  void Cancel(DispatcherRequest::Cancel request) ABSL_LOCKS_EXCLUDED(mu_) {
    // Kill the worker.
    absl::MutexLock lock(&mu_);
    for (const auto& [pid, execution_token] : pid_to_execution_token_) {
      if (request.execution_token() == execution_token) {
        if (::kill(pid, SIGKILL) == -1) {
          PLOG(INFO) << "kill(" << pid << ", SIGKILL)";
        }
        break;
      }
    }
  }

  absl::Status SaveNewBinary(const std::filesystem::path& binary_path,
                             std::string_view binary_content) {
    const int fd =
        ::open(binary_path.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0500);
    if (fd == -1) {
      return absl::ErrnoToStatus(errno, "open()");
    }
    if (::write(fd, binary_content.data(), binary_content.size()) !=
        binary_content.size()) {
      return absl::ErrnoToStatus(errno, "write()");
    }

    // Flush the file to ensure the executable is closed for writing before
    // the `exec` call.
    if (::fsync(fd) == -1) {
      return absl::ErrnoToStatus(errno, "fsync()");
    }
    if (::close(fd) == -1) {
      return absl::ErrnoToStatus(errno, "close()");
    }
    return absl::OkStatus();
  }

  absl::Status CreateHardLinkForExistingBinary(
      const std::filesystem::path& binary_path,
      std::string_view source_bin_code_token) {
    const std::filesystem::path existing_binary_path =
        progdir_ / source_bin_code_token / source_bin_code_token;
    if (!std::filesystem::exists(existing_binary_path)) {
      return absl::FailedPreconditionError(absl::StrCat(
          "Expected binary ", existing_binary_path.native(), " not found"));
    } else if (!std::filesystem::is_regular_file(existing_binary_path)) {
      return absl::InternalError(absl::StrCat(
          "File ", existing_binary_path.native(), " not a regular file"));
    }
    std::filesystem::create_hard_link(existing_binary_path, binary_path);
    return absl::OkStatus();
  }

  absl::Status CreateWorkerPool(std::filesystem::path binary_path,
                                std::string code_token, const int num_workers,
                                const bool enable_log_egress)
      ABSL_LOCKS_EXCLUDED(mu_) {
    {
      absl::MutexLock lock(&mu_);
      active_code_token_to_pids_[code_token].reserve(num_workers);
    }
    for (int i = 0; i < num_workers - 1; ++i) {
      // TODO: b/375622989 - Refactor run_workers to make the code more
      // coherent.
      PS_ASSIGN_OR_RETURN(
          PidExecutionTokenAndPivotRootDir
              pid_execution_token_and_pivot_root_dir,
          ConnectSendCloneAndExec(code_token, binary_path.native(),
                                  enable_log_egress));
      {
        absl::MutexLock lock(&mu_);
        active_code_token_to_pids_[code_token].insert(
            pid_execution_token_and_pivot_root_dir.pid);
        pid_to_execution_token_[pid_execution_token_and_pivot_root_dir.pid] =
            std::move(pid_execution_token_and_pivot_root_dir.execution_token);
        ++code_token_to_thread_count_[code_token];
      }
      std::thread(
          &WorkerRunner::ReloaderImpl, this,
          pid_execution_token_and_pivot_root_dir.pid,
          std::move(pid_execution_token_and_pivot_root_dir.pivot_root_dir),
          code_token, binary_path, enable_log_egress)
          .detach();
    }

    // Start n-th worker out of loop.
    // TODO: b/375622989 - Refactor run_workers to make the code more
    // coherent.
    PS_ASSIGN_OR_RETURN(
        PidExecutionTokenAndPivotRootDir pid_execution_token_and_pivot_root_dir,
        ConnectSendCloneAndExec(code_token, binary_path.native(),
                                enable_log_egress));
    {
      absl::MutexLock lock(&mu_);
      active_code_token_to_pids_[code_token].insert(
          pid_execution_token_and_pivot_root_dir.pid);
      pid_to_execution_token_[pid_execution_token_and_pivot_root_dir.pid] =
          std::move(pid_execution_token_and_pivot_root_dir.execution_token);
      ++code_token_to_thread_count_[code_token];
    }
    std::thread(
        &WorkerRunner::ReloaderImpl, this,
        pid_execution_token_and_pivot_root_dir.pid,
        std::move(pid_execution_token_and_pivot_root_dir.pivot_root_dir),
        std::move(code_token), std::move(binary_path), enable_log_egress)
        .detach();
    return absl::OkStatus();
  }

  const std::string socket_name_;
  const std::vector<std::string> mounts_;
  const std::string log_dir_name_;
  const int dev_null_fd_;
  const std::filesystem::path& progdir_;
  absl::Mutex mu_;
  absl::flat_hash_map<int, std::string> pid_to_execution_token_
      ABSL_GUARDED_BY(mu_);
  absl::flat_hash_map<std::string, absl::flat_hash_set<int>>
      active_code_token_to_pids_ ABSL_GUARDED_BY(mu_);
  absl::flat_hash_map<std::string, int> code_token_to_thread_count_
      ABSL_GUARDED_BY(mu_);
};
}  // namespace

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  LOG(INFO) << "Starting up.";
  const std::filesystem::path progdir =
      std::filesystem::temp_directory_path() /
      ToString(google::scp::core::common::Uuid::GenerateUuid());
  if (std::error_code ec; !std::filesystem::create_directories(progdir, ec)) {
    LOG(ERROR) << "Failed to create " << progdir << ": " << ec;
    return -1;
  }
  absl::Cleanup progdir_cleanup = [&progdir] {
    if (std::error_code ec; std::filesystem::remove_all(progdir, ec) ==
                            static_cast<std::uintmax_t>(-1)) {
      LOG(ERROR) << "Failed to remove " << progdir << ": " << ec;
    }
  };
  const int dev_null_fd = ::open("/dev/null", O_WRONLY);
  if (dev_null_fd < 0) {
    PLOG(ERROR) << "open failed for /dev/null";
    return -1;
  }
  WorkerRunner runner(absl::GetFlag(FLAGS_socket_name),
                      absl::GetFlag(FLAGS_mounts), absl::GetFlag(FLAGS_log_dir),
                      dev_null_fd, progdir);
  if (const absl::Status status = runner.Run(); !status.ok()) {
    LOG(ERROR) << status;
    return -1;
  }
  return 0;
}
