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

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>

#include "absl/base/attributes.h"
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
#include "src/roma/byob/dispatcher/dispatcher.grpc.pb.h"
#include "src/roma/byob/dispatcher/dispatcher.h"
#include "src/roma/byob/dispatcher/dispatcher.pb.h"
#include "src/util/status_macro/status_macros.h"
#include "src/util/status_macro/status_util.h"

ABSL_FLAG(std::string, control_socket_name, "/sockdir/xyzw.sock",
          "Socket for host to run_workers communication");
ABSL_FLAG(std::string, udf_socket_name, "/sockdir/abcd.sock",
          "socket for host to UDF communication");

namespace {
using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using ::privacy_sandbox::server_common::byob::CancelRequest;
using ::privacy_sandbox::server_common::byob::CancelResponse;
using ::privacy_sandbox::server_common::byob::DeleteBinaryRequest;
using ::privacy_sandbox::server_common::byob::DeleteBinaryResponse;
using ::privacy_sandbox::server_common::byob::kNumTokenBytes;
using ::privacy_sandbox::server_common::byob::LoadBinaryRequest;
using ::privacy_sandbox::server_common::byob::LoadBinaryResponse;
using ::privacy_sandbox::server_common::byob::WorkerRunnerService;

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

class WorkerRunner final : public WorkerRunnerService::Service {
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

  grpc::Status LoadBinary(grpc::ServerContext* /*context*/,
                          const LoadBinaryRequest* request,
                          LoadBinaryResponse* /*response*/)
      ABSL_LOCKS_EXCLUDED(mu_) {
    return privacy_sandbox::server_common::FromAbslStatus(Load(*request));
  }

  grpc::Status DeleteBinary(grpc::ServerContext* /*context*/,
                            const DeleteBinaryRequest* request,
                            DeleteBinaryResponse* /*response*/)
      ABSL_LOCKS_EXCLUDED(mu_) {
    Delete(request->code_token());
    return grpc::Status::OK;
  }

  grpc::Status Cancel(grpc::ServerContext* /*context*/,
                      const CancelRequest* request,
                      CancelResponse* /*response*/) ABSL_LOCKS_EXCLUDED(mu_) {
    Cancel(request->execution_token());
    return grpc::Status::OK;
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

  absl::Status Load(const LoadBinaryRequest& request) ABSL_LOCKS_EXCLUDED(mu_) {
    const std::filesystem::path binary_dir = progdir_ / request.code_token();
    if (std::error_code ec;
        !std::filesystem::create_directory(binary_dir, ec)) {
      return absl::InternalError(absl::StrCat(
          "Failed to create ", binary_dir.native(), ": ", ec.message()));
    }
    std::filesystem::path binary_path = binary_dir / request.code_token();
    if (request.has_binary_content()) {
      PS_RETURN_IF_ERROR(SaveNewBinary(binary_path, request.binary_content()));
    } else if (request.has_source_bin_code_token()) {
      PS_RETURN_IF_ERROR(CreateHardLinkForExistingBinary(
          binary_path, request.source_bin_code_token()));
    } else {
      return absl::InternalError("Failed to load binary");
    }
    return CreateWorkerPool(std::move(binary_path), request.code_token(),
                            request.num_workers());
  }

  void Delete(std::string_view request_code_token) ABSL_LOCKS_EXCLUDED(mu_) {
    // Kill all workers and mark for removal.
    absl::MutexLock lock(&mu_);
    if (const auto it = active_code_token_to_pids_.find(request_code_token);
        it != active_code_token_to_pids_.end()) {
      for (const int pid : it->second) {
        if (::kill(pid, SIGKILL) == -1) {
          PLOG(INFO) << "kill(" << pid << ", SIGKILL)";
        }
      }
      active_code_token_to_pids_.erase(it);
    }
  }

  void Cancel(std::string_view request_execution_token)
      ABSL_LOCKS_EXCLUDED(mu_) {
    // Kill the worker.
    absl::MutexLock lock(&mu_);
    for (const auto& [pid, execution_token] : pid_to_execution_token_) {
      if (request_execution_token == execution_token) {
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

ABSL_CONST_INIT absl::Mutex signal_mu{absl::kConstInit};
bool signal_flag = false;
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
  WorkerRunner runner(absl::GetFlag(FLAGS_udf_socket_name), progdir);
  grpc::EnableDefaultHealthCheckService(true);
  std::unique_ptr<grpc::Server> server =
      grpc::ServerBuilder()
          .AddListeningPort(
              absl::StrCat("unix:", absl::GetFlag(FLAGS_control_socket_name)),
              grpc::InsecureServerCredentials())
          .SetMaxSendMessageSize(200'000'000)
          .SetMaxReceiveMessageSize(200'000'000)
          .AddChannelArgument(GRPC_ARG_KEEPALIVE_TIME_MS,
                              absl::ToInt64Milliseconds(absl::Minutes(10)))
          .AddChannelArgument(GRPC_ARG_KEEPALIVE_TIMEOUT_MS,
                              absl::ToInt64Milliseconds(absl::Seconds(20)))
          // TODO(b/378743613): Failed to set zerocopy options on socket for
          // non-Gvisor
          .AddChannelArgument(GRPC_ARG_TCP_TX_ZEROCOPY_ENABLED, 0)
          .RegisterService(&runner)
          .BuildAndStart();
  server->GetHealthCheckService()->SetServingStatus(true);

  // Run until `SIGTERM` received but then exit normally.
  ::signal(
      SIGTERM, +[](int /*sig*/) {
        absl::MutexLock lock(&signal_mu);
        signal_flag = true;
      });
  {
    absl::MutexLock lock(&signal_mu);
    signal_mu.Await(absl::Condition(&signal_flag));
  }
  server->GetHealthCheckService()->SetServingStatus(false);
}
