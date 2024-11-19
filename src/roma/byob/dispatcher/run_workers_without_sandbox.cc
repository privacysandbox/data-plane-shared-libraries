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

ABSL_FLAG(std::string, socket_name, "/sockdir/abcd.sock",
          "Server socket for reaching Roma app API");

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
  std::string_view execution_token;
  std::string_view socket_name;
  std::string_view code_token;
  std::string_view binary_path;
};

int WorkerImpl(void* arg) {
  const WorkerImplArg& worker_impl_arg = *static_cast<WorkerImplArg*>(arg);
  const int rpc_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  PCHECK(rpc_fd != -1);
  if (!ConnectToPath(rpc_fd, worker_impl_arg.socket_name)) {
    PLOG(INFO) << "connect() to " << worker_impl_arg.socket_name << " failed";
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
struct PidAndExecutionToken {
  int pid;
  std::string execution_token;
};

// Returns `std::nullopt` when workers can no longer be created: in virtually
// all cases, this is because Roma is shutting down and is not an error.
std::optional<PidAndExecutionToken> ConnectSendCloneAndExec(
    std::string_view socket_name, std::string_view code_token,
    std::string_view binary_path) {
  std::string execution_token =
      ToString(google::scp::core::common::Uuid::GenerateUuid());
  WorkerImplArg worker_impl_arg{
      .execution_token = execution_token,
      .socket_name = socket_name,
      .code_token = code_token,
      .binary_path = binary_path,
  };

  // Explicitly 16-byte align the stack. Otherwise, `clone` on aarch64 may hang
  // or the process may receive SIGBUS (depending on the size of the stack
  // before this function call). Overprovisions stack by at most 15 bytes (of
  // 2^10 bytes) where unneeded.
  // https://community.arm.com/arm-community-blogs/b/architectures-and-processors-blog/posts/using-the-stack-in-aarch32-and-aarch64
  alignas(16) char stack[1 << 20];
  const pid_t pid = ::clone(WorkerImpl, stack + sizeof(stack),
                            CLONE_VM | CLONE_VFORK | SIGCHLD, &worker_impl_arg);
  if (pid == -1) {
    PLOG(ERROR) << "clone()";
    return std::nullopt;
  }
  return PidAndExecutionToken{
      .pid = pid,
      .execution_token = std::move(execution_token),
  };
}
}  // namespace

int main(int argc, char** argv) {
  std::vector<char*> args = absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  LOG(INFO) << "Starting up.";
  const std::string socket_name = absl::GetFlag(FLAGS_socket_name);
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
  const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd == -1) {
    PLOG(ERROR) << "socket()";
    return -1;
  }
  if (!ConnectToPath(fd, socket_name)) {
    PLOG(ERROR) << "connect() to " << socket_name << " failed";
    return -1;
  }
  absl::Mutex mu;
  absl::flat_hash_map<int, std::string>
      pid_to_execution_token;  // Guarded by mu.
  absl::flat_hash_map<std::string, absl::flat_hash_set<int>>
      active_code_token_to_pids;  // Guarded by mu.
  absl::flat_hash_map<std::string, int>
      code_token_to_thread_count;  // Guarded by mu.
  auto reload_fn = [&socket_name, &mu, &pid_to_execution_token,
                    &active_code_token_to_pids, &code_token_to_thread_count](
                       int pid, const std::string code_token,
                       const std::string binary_path) {
    while (true) {
      int status;
      if (::waitpid(pid, &status, /*options=*/0) == -1) {
        PLOG(INFO) << "waitpid()";
      }
      {
        absl::MutexLock lock(&mu);
        pid_to_execution_token.erase(pid);
        if (const auto it = active_code_token_to_pids.find(code_token);
            it == active_code_token_to_pids.end()) {
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
      std::optional<PidAndExecutionToken> pid_and_execution_token =
          ConnectSendCloneAndExec(socket_name, code_token, binary_path);
      if (!pid_and_execution_token.has_value()) {
        break;
      }
      pid = pid_and_execution_token->pid;
      {
        absl::MutexLock lock(&mu);
        pid_to_execution_token[pid] =
            std::move(pid_and_execution_token->execution_token);
        if (const auto it = active_code_token_to_pids.find(code_token);
            it == active_code_token_to_pids.end()) {
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
    absl::MutexLock lock(&mu);
    const auto it = code_token_to_thread_count.find(code_token);
    CHECK(it != code_token_to_thread_count.end());

    // Delete binary if this is the last worker for the code token.
    if (--it->second == 0) {
      if (std::error_code ec; std::filesystem::remove_all(binary_dir, ec) ==
                              static_cast<std::uintmax_t>(-1)) {
        LOG(INFO) << "Failed to remove " << binary_dir.native() << ": " << ec;
      }
      code_token_to_thread_count.erase(it);
    }
  };
  absl::Cleanup cleanup = [&] {
    LOG(INFO) << "Shutting down.";
    absl::MutexLock lock(&mu);
    active_code_token_to_pids.clear();

    // Kill extant workers before exit.
    for (const auto& [pid, _] : pid_to_execution_token) {
      if (::kill(pid, SIGKILL) == -1) {
        // If the process has already terminated, degrade error to a log.
        if (errno == ESRCH) {
          PLOG(INFO) << "kill(" << pid << ", SIGKILL)";
        } else {
          PLOG(ERROR) << "kill(" << pid << ", SIGKILL)";
        }
      }
    }
    auto fn = [&mu, &code_token_to_thread_count] {
      mu.AssertReaderHeld();
      return code_token_to_thread_count.empty();
    };
    mu.Await(absl::Condition(&fn));
  };
  FileInputStream input(fd);
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
      if (const auto it = active_code_token_to_pids.find(
              request.delete_binary().code_token());
          it != active_code_token_to_pids.end()) {
        for (const int pid : it->second) {
          if (::kill(pid, SIGKILL) == -1) {
            PLOG(INFO) << "kill(" << pid << ", SIGKILL)";
          }
        }
        active_code_token_to_pids.erase(it);
      }
      continue;
    }
    if (request.has_cancel()) {
      // Kill the worker.
      absl::MutexLock lock(&mu);
      for (const auto& [pid, execution_token] : pid_to_execution_token) {
        if (request.cancel().execution_token() == execution_token) {
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
      const std::filesystem::path existing_binary_path =
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
    {
      absl::MutexLock lock(&mu);
      active_code_token_to_pids[request.load_binary().code_token()].reserve(
          request.load_binary().num_workers());
    }
    for (int i = 0; i < request.load_binary().num_workers() - 1; ++i) {
      std::optional<PidAndExecutionToken> pid_and_execution_token =
          ConnectSendCloneAndExec(socket_name,
                                  request.load_binary().code_token(),
                                  binary_path.native());
      if (!pid_and_execution_token.has_value()) {
        return -1;
      }
      {
        absl::MutexLock lock(&mu);
        active_code_token_to_pids[request.load_binary().code_token()].insert(
            pid_and_execution_token->pid);
        pid_to_execution_token[pid_and_execution_token->pid] =
            std::move(pid_and_execution_token->execution_token);
        ++code_token_to_thread_count[request.load_binary().code_token()];
      }
      std::thread(reload_fn, pid_and_execution_token->pid,
                  request.load_binary().code_token(), binary_path)
          .detach();
    }

    // Start n-th worker out of loop.
    std::optional<PidAndExecutionToken> pid_and_execution_token =
        ConnectSendCloneAndExec(socket_name, request.load_binary().code_token(),
                                binary_path.native());
    if (!pid_and_execution_token.has_value()) {
      return -1;
    }
    {
      absl::MutexLock lock(&mu);
      active_code_token_to_pids[request.load_binary().code_token()].insert(
          pid_and_execution_token->pid);
      pid_to_execution_token[pid_and_execution_token->pid] =
          std::move(pid_and_execution_token->execution_token);
      ++code_token_to_thread_count[request.load_binary().code_token()];
    }
    std::thread(reload_fn, pid_and_execution_token->pid,
                std::move(*request.mutable_load_binary()->mutable_code_token()),
                binary_path)
        .detach();
  }
  return 0;
}
