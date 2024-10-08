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
#include <string.h>
#include <sys/capability.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <filesystem>
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
#include "src/roma/byob/dispatcher/dispatcher.pb.h"

ABSL_FLAG(std::string, socket_name, "/sockdir/abcd.sock",
          "Server socket for reaching Roma app API");
ABSL_FLAG(std::vector<std::string>, mounts,
          std::vector<std::string>({"/lib", "/lib64"}),
          "Mounts containing dependencies needed by the binary");

namespace {
using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using ::privacy_sandbox::server_common::byob::LoadRequest;

bool ConnectToPath(int fd, std::string_view socket_name) {
  ::sockaddr_un sa = {
      .sun_family = AF_UNIX,
  };
  socket_name.copy(sa.sun_path, sizeof(sa.sun_path));
  return ::connect(fd, reinterpret_cast<::sockaddr*>(&sa), SUN_LEN(&sa)) == 0;
}
struct WorkerImplArg {
  absl::Span<const std::string> mounts;
  std::string_view pivot_root_dir;
  int rpc_fd;
  std::string_view code_token;
  std::string_view binary_path;
  const int dev_null_fd;
};

void SetPrctlOptions(absl::Span<const std::pair<int, int>> option_arg_pairs) {
  for (const auto& [option, arg] : option_arg_pairs) {
    if (::prctl(option, arg) < 0) {
      PLOG(FATAL) << "Failed prctl(" << option << ", " << arg << ")";
    }
  }
}

int WorkerImpl(void* arg) {
  const WorkerImplArg& worker_impl_arg = *static_cast<WorkerImplArg*>(arg);
  PCHECK(::write(worker_impl_arg.rpc_fd, worker_impl_arg.code_token.data(),
                 36) == 36);

  // Set up restricted filesystem for worker using pivot_root
  // pivot_root doesn't work under an MS_SHARED mount point.
  // https://man7.org/linux/man-pages/man2/pivot_root.2.html.
  PCHECK(::mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) == 0)
      << "Failed to mount /";
  for (const std::string& mount : worker_impl_arg.mounts) {
    const std::string target =
        absl::StrCat(worker_impl_arg.pivot_root_dir, mount);
    CHECK(std::filesystem::create_directories(target));
    PCHECK(::mount(mount.c_str(), target.c_str(), nullptr, MS_BIND, nullptr) ==
           0)
        << "Failed to mount " << mount;
  }
  const std::string binary_dir =
      std::filesystem::path(worker_impl_arg.binary_path).parent_path();
  {
    const std::string target =
        absl::StrCat(worker_impl_arg.pivot_root_dir, binary_dir);
    CHECK(std::filesystem::create_directories(target));
    PCHECK(::mount(binary_dir.c_str(), target.c_str(), nullptr, MS_BIND,
                   nullptr) == 0);
  }

  // MS_REC needed here to get other mounts (/lib, /lib64 etc)
  PCHECK(::mount(worker_impl_arg.pivot_root_dir.data(),
                 worker_impl_arg.pivot_root_dir.data(), "bind",
                 MS_REC | MS_BIND, nullptr) == 0)
      << "Failed to mount " << worker_impl_arg.pivot_root_dir;
  PCHECK(::mount(worker_impl_arg.pivot_root_dir.data(),
                 worker_impl_arg.pivot_root_dir.data(), "bind",
                 MS_REC | MS_SLAVE, nullptr) == 0)
      << "Failed to mount " << worker_impl_arg.pivot_root_dir;
  {
    const std::string pivot_dir =
        absl::StrCat(worker_impl_arg.pivot_root_dir, "/pivot");
    CHECK(std::filesystem::create_directories(pivot_dir));
    PCHECK(::syscall(SYS_pivot_root, worker_impl_arg.pivot_root_dir.data(),
                     pivot_dir.c_str()) == 0);
  }
  PCHECK(::chdir("/") == 0);
  PCHECK(::umount2("/pivot", MNT_DETACH) == 0);
  for (const std::string& mount : worker_impl_arg.mounts) {
    PCHECK(::mount(mount.c_str(), mount.c_str(), nullptr, MS_REMOUNT | MS_BIND,
                   nullptr) == 0)
        << "Failed to mount " << mount;
  }
  PCHECK(::mount(binary_dir.c_str(), binary_dir.c_str(), nullptr,
                 MS_REMOUNT | MS_BIND, nullptr) == 0);

  // Exec binary.
  const std::string connection_fd = [](const int fd) {
    const int connection_fd = ::dup(fd);
    PCHECK(connection_fd != -1);
    return absl::StrCat(connection_fd);
  }(worker_impl_arg.rpc_fd);
  SetPrctlOptions({
      {PR_CAPBSET_DROP, CAP_SYS_ADMIN},
      {PR_CAPBSET_DROP, CAP_SETPCAP},
      {PR_SET_PDEATHSIG, SIGHUP},
  });
  {
    PCHECK(::dup2(worker_impl_arg.dev_null_fd, STDOUT_FILENO) != -1);
    PCHECK(::dup2(worker_impl_arg.dev_null_fd, STDERR_FILENO) != -1);
  }
  ::execl(worker_impl_arg.binary_path.data(),
          worker_impl_arg.binary_path.data(), connection_fd.c_str(), nullptr);
  PLOG(FATAL) << "exec '" << worker_impl_arg.binary_path << "' failed";
}
struct PidAndPivotRootDir {
  int pid;
  std::string pivot_root_dir;
};

// Returns `std::nullopt` when workers can no longer be created: in virtually
// all cases, this is because Roma is shutting down and is not an error.
std::optional<PidAndPivotRootDir> ConnectSendCloneAndExec(
    absl::Span<const std::string> mounts, std::string_view socket_name,
    std::string_view code_token, std::string_view binary_path,
    const int dev_null_fd) {
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
  char tmp_file[] = "/tmp/roma_app_server_XXXXXX";
  const char* pivot_root_dir = ::mkdtemp(tmp_file);
  if (pivot_root_dir == nullptr) {
    PLOG(ERROR) << "mkdtemp()";
    return std::nullopt;
  }
  WorkerImplArg worker_impl_arg{
      .mounts = mounts,
      .pivot_root_dir = pivot_root_dir,
      .rpc_fd = rpc_fd,
      .code_token = code_token,
      .binary_path = binary_path,
      .dev_null_fd = dev_null_fd,
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
  return PidAndPivotRootDir{
      .pid = pid,
      .pivot_root_dir = std::string(pivot_root_dir),
  };
}
}  // namespace

int main(int argc, char** argv) {
  std::vector<char*> args = absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  LOG(INFO) << "Starting up.";
  const std::string socket_name = absl::GetFlag(FLAGS_socket_name);
  const std::vector<std::string> mounts = absl::GetFlag(FLAGS_mounts);
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
    std::string code_token;
    std::string binary_path;
  };

  absl::Mutex mu;
  absl::flat_hash_map<int, UdfInstanceMetadata> pid_to_udf;  // Guarded by mu.
  bool shutdown = false;                                     // Guarded by mu.
  std::thread reloader([&socket_name, &mounts, &mu, &pid_to_udf, &shutdown,
                        &dev_null_fd] {
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
      {
        int status;

        // Wait for any worker to end.
        const int pid = ::waitpid(-1, &status, /*options=*/0);
        PCHECK(pid != -1);
        absl::MutexLock lock(&mu);
        const auto it = pid_to_udf.find(pid);
        if (it == pid_to_udf.end()) {
          LOG(ERROR) << "waitpid() returned unknown pid=" << pid;
          continue;
        }
        udf = std::move(it->second);
        pid_to_udf.erase(it);
        if (!WIFEXITED(status)) {
          if (WIFSIGNALED(status)) {
            LOG(ERROR) << "Process pid=" << pid
                       << " terminated (signal=" << WTERMSIG(status)
                       << ", coredump=" << WCOREDUMP(status) << ")";
          } else {
            LOG(ERROR) << "Process pid=" << pid << " did not exit";
          }
        } else if (const int exit_code = WEXITSTATUS(status); exit_code != 0) {
          LOG(ERROR) << "Process pid=" << pid << " exit_code=" << exit_code;
        }
      }
      // Start a new worker with the same UDF as the most-recently ended worker.
      std::optional<PidAndPivotRootDir> pid_and_pivot_root_dir =
          ConnectSendCloneAndExec(mounts, socket_name, udf.code_token,
                                  udf.binary_path, dev_null_fd);
      CHECK(std::filesystem::remove_all(udf.pivot_root_dir));
      if (!pid_and_pivot_root_dir.has_value()) {
        return;
      }
      udf.pivot_root_dir = std::move(pid_and_pivot_root_dir->pivot_root_dir);
      absl::MutexLock lock(&mu);
      CHECK(pid_to_udf.insert({pid_and_pivot_root_dir->pid, std::move(udf)})
                .second);
    }
  });
  absl::Cleanup cleanup = [&] {
    LOG(INFO) << "Shutting down.";
    {
      absl::MutexLock lock(&mu);
      shutdown = true;
    }
    reloader.join();

    // Kill extant workers before exit.
    for (const auto& [pid, udf] : pid_to_udf) {
      if (::kill(pid, SIGKILL) == -1) {
        PLOG(ERROR) << "kill(" << pid << ", SIGKILL)";
      }
      if (::waitpid(pid, /*status=*/nullptr, /*options=*/0) == -1) {
        PLOG(ERROR) << "waitpid(" << pid << ", nullptr, 0)";
      }
      if (std::error_code ec;
          !std::filesystem::remove_all(udf.pivot_root_dir, ec)) {
        LOG(ERROR) << "Failed to remove " << udf.pivot_root_dir << ": " << ec;
      }
    }
    if (::close(dev_null_fd) == -1) {
      PLOG(ERROR) << "close(" << dev_null_fd << ")";
    }
  };
  FileInputStream input(rpc_fd);
  while (true) {
    LoadRequest request;
    if (!ParseDelimitedFromZeroCopyStream(&request, &input, nullptr)) {
      break;
    }
    const std::filesystem::path binary_dir = progdir / request.code_token();
    if (std::error_code ec;
        !std::filesystem::create_directory(binary_dir, ec)) {
      LOG(ERROR) << "Failed to create " << binary_dir << ": " << ec;
      return -1;
    }
    const std::filesystem::path binary_path = binary_dir / request.code_token();
    {
      const int fd =
          ::open(binary_path.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0500);
      if (fd == -1) {
        PLOG(ERROR) << "open()";
        return -1;
      }
      if (::write(fd, request.binary_content().c_str(),
                  request.binary_content().size()) !=
          request.binary_content().size()) {
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
    }
    for (int i = 0; i < request.n_workers() - 1; ++i) {
      std::optional<PidAndPivotRootDir> pid_and_pivot_root_dir =
          ConnectSendCloneAndExec(mounts, socket_name, request.code_token(),
                                  binary_path.native(), dev_null_fd);
      if (!pid_and_pivot_root_dir.has_value()) {
        return -1;
      }
      UdfInstanceMetadata udf{
          .pivot_root_dir = std::move(pid_and_pivot_root_dir->pivot_root_dir),
          .code_token = request.code_token(),
          .binary_path = binary_path,
      };
      absl::MutexLock lock(&mu);
      pid_to_udf[pid_and_pivot_root_dir->pid] = std::move(udf);
    }

    // Start n-th worker out of loop.
    std::optional<PidAndPivotRootDir> pid_and_pivot_root_dir =
        ConnectSendCloneAndExec(mounts, socket_name, request.code_token(),
                                binary_path.native(), dev_null_fd);
    if (!pid_and_pivot_root_dir.has_value()) {
      return -1;
    }
    UdfInstanceMetadata udf{
        .pivot_root_dir = std::move(pid_and_pivot_root_dir->pivot_root_dir),
        .code_token = std::move(*request.mutable_code_token()),
        .binary_path = binary_path,
    };
    absl::MutexLock lock(&mu);
    pid_to_udf[pid_and_pivot_root_dir->pid] = std::move(udf);
  }
  return 0;
}
