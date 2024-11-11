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
#include <sys/socket.h>
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

#include "absl/base/thread_annotations.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/synchronization/mutex.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/core/common/uuid/uuid.h"
#include "src/roma/byob/dispatcher/dispatcher.h"
#include "src/roma/byob/dispatcher/dispatcher.pb.h"
#include "src/util/status_macro/status_macros.h"

ABSL_FLAG(std::string, socket_name, "/sockdir/abcd.sock",
          "Server socket for reaching Roma app API");

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
  std::string_view execution_token;
  std::string_view socket_name;
  std::string_view code_token;
  std::string_view binary_path;
};

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

  // The maximum int value is 10 digits and `snprintf` adds a null terminator.
  char connection_fd[11];
  PCHECK(::snprintf(connection_fd, sizeof(connection_fd), "%d", rpc_fd) > 0);

  // Exec binary.
  ::execl(worker_impl_arg.binary_path.data(),
          worker_impl_arg.binary_path.data(), connection_fd, nullptr);
  PLOG(FATAL) << "exec '" << worker_impl_arg.binary_path << "' failed";
}

class WorkerRunner final {
 public:
  WorkerRunner(std::string socket_name, const std::filesystem::path& progdir)
      : socket_name_(std::move(socket_name)), progdir_(progdir) {}

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
          request.load_binary().num_workers()));
    }
  }

 private:
  struct PidAndExecutionToken {
    int pid;
    std::string execution_token;
  };

  absl::StatusOr<PidAndExecutionToken> ConnectSendCloneAndExec(
      std::string_view code_token, std::string_view binary_path) {
    std::string execution_token =
        ToString(google::scp::core::common::Uuid::GenerateUuid());
    WorkerImplArg worker_impl_arg{
        .execution_token = execution_token,
        .socket_name = socket_name_,
        .code_token = code_token,
        .binary_path = binary_path,
    };

    // Explicitly 16-byte align the stack. Otherwise, `clone` on aarch64 may
    // hang or the process may receive SIGBUS (depending on the size of the
    // stack before this function call). Overprovisions stack by at most 15
    // bytes (of 2^10 bytes) where unneeded.
    // https://community.arm.com/arm-community-blogs/b/architectures-and-processors-blog/posts/using-the-stack-in-aarch32-and-aarch64
    alignas(16) char stack[1 << 20];
    const pid_t pid =
        ::clone(WorkerImpl, stack + sizeof(stack),
                CLONE_VM | CLONE_VFORK | SIGCHLD, &worker_impl_arg);
    if (pid == -1) {
      return absl::ErrnoToStatus(errno, "clone()");
    }
    return PidAndExecutionToken{
        .pid = pid,
        .execution_token = std::move(execution_token),
    };
  }

  void ReloaderImpl(int pid, const std::string code_token,
                    const std::string binary_path) ABSL_LOCKS_EXCLUDED(mu_) {
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
      absl::StatusOr<PidAndExecutionToken> pid_and_execution_token =
          ConnectSendCloneAndExec(code_token, binary_path);
      if (!pid_and_execution_token.ok()) {
        break;
      }
      pid = pid_and_execution_token->pid;
      {
        absl::MutexLock lock(&mu_);
        pid_to_execution_token_[pid] =
            std::move(pid_and_execution_token->execution_token);
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
                                std::string code_token, const int num_workers)
      ABSL_LOCKS_EXCLUDED(mu_) {
    {
      absl::MutexLock lock(&mu_);
      active_code_token_to_pids_[code_token].reserve(num_workers);
    }
    for (int i = 0; i < num_workers - 1; ++i) {
      PS_ASSIGN_OR_RETURN(
          PidAndExecutionToken pid_and_execution_token,
          ConnectSendCloneAndExec(code_token, binary_path.native()));
      {
        absl::MutexLock lock(&mu_);
        active_code_token_to_pids_[code_token].insert(
            pid_and_execution_token.pid);
        pid_to_execution_token_[pid_and_execution_token.pid] =
            std::move(pid_and_execution_token.execution_token);
        ++code_token_to_thread_count_[code_token];
      }
      std::thread(&WorkerRunner::ReloaderImpl, this,
                  pid_and_execution_token.pid, code_token, binary_path)
          .detach();
    }

    // Start n-th worker out of loop.
    PS_ASSIGN_OR_RETURN(
        PidAndExecutionToken pid_and_execution_token,
        ConnectSendCloneAndExec(code_token, binary_path.native()));
    {
      absl::MutexLock lock(&mu_);
      active_code_token_to_pids_[code_token].insert(
          pid_and_execution_token.pid);
      pid_to_execution_token_[pid_and_execution_token.pid] =
          std::move(pid_and_execution_token.execution_token);
      ++code_token_to_thread_count_[code_token];
    }
    std::thread(&WorkerRunner::ReloaderImpl, this, pid_and_execution_token.pid,
                std::move(code_token), std::move(binary_path))
        .detach();
    return absl::OkStatus();
  }

  const std::string socket_name_;
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
  WorkerRunner runner(absl::GetFlag(FLAGS_socket_name), progdir);
  if (const absl::Status status = runner.Run(); !status.ok()) {
    LOG(ERROR) << status;
    return -1;
  }
  return 0;
}
