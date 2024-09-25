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
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/core/common/uuid/uuid.h"
#include "src/roma/byob/dispatcher/dispatcher.pb.h"

ABSL_FLAG(std::string, socket_name, "/sockdir/abcd.sock",
          "Server socket for reaching Roma app API");

namespace {
using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using ::privacy_sandbox::server_common::byob::DispatcherRequest;

bool ConnectToPath(int fd, std::string_view socket_name) {
  ::sockaddr_un sa = {
      .sun_family = AF_UNIX,
  };
  socket_name.copy(sa.sun_path, sizeof(sa.sun_path));
  return ::connect(fd, reinterpret_cast<::sockaddr*>(&sa), SUN_LEN(&sa)) == 0;
}
struct WorkerImplArg {
  int fd;
  std::string_view code_token;
  std::string_view binary_path;
};

int WorkerImpl(void* arg) {
  const WorkerImplArg& worker_impl_arg = *static_cast<WorkerImplArg*>(arg);
  PCHECK(::write(worker_impl_arg.fd, worker_impl_arg.code_token.data(), 36) ==
         36);

  // Exec binary.
  const std::string connection_fd = [worker_impl_arg] {
    const int connection_fd = ::dup(worker_impl_arg.fd);
    PCHECK(connection_fd != -1);
    return absl::StrCat(connection_fd);
  }();
  ::execl(worker_impl_arg.binary_path.data(),
          worker_impl_arg.binary_path.data(), connection_fd.c_str(), nullptr);
  PLOG(FATAL) << "exec '" << worker_impl_arg.binary_path << "' failed";
}

// Returns `std::nullopt` when workers can no longer be created: in virtually
// all cases, this is because Roma is shutting down and is not an error.
std::optional<int> ConnectSendCloneAndExec(std::string_view socket_name,
                                           std::string_view code_token,
                                           std::string_view binary_path) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd == -1) {
    PLOG(ERROR) << "socket()";
    return std::nullopt;
  }
  absl::Cleanup cleanup = [fd] {
    if (::close(fd) == -1) {
      PLOG(ERROR) << "close()";
    }
  };
  if (!ConnectToPath(fd, socket_name)) {
    PLOG(INFO) << "connect() to " << socket_name << " failed";
    return std::nullopt;
  }
  WorkerImplArg worker_impl_arg{fd, code_token, binary_path};

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
  return pid;
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
    if (std::error_code ec; !std::filesystem::remove_all(progdir, ec)) {
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
  struct UdfInstanceMetadata {
    std::string code_token;
    std::string binary_path;
    bool marked_for_deletion = false;
  };

  absl::Mutex mu;
  absl::flat_hash_map<int, UdfInstanceMetadata> pid_to_udf;  // Guarded by mu.
  bool shutdown = false;                                     // Guarded by mu.
  std::thread reloader([&socket_name, &mu, &pid_to_udf, &shutdown] {
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
        if (udf.marked_for_deletion) {
          std::filesystem::remove_all(
              std::filesystem::path(udf.binary_path).parent_path());
          continue;
        }

        // Start a new worker with the same UDF as the most-recently ended
        // worker.
        const std::optional<int> new_pid = ConnectSendCloneAndExec(
            socket_name, udf.code_token, udf.binary_path);
        if (!new_pid.has_value()) {
          return;
        }
        CHECK(pid_to_udf.try_emplace(*new_pid, std::move(udf)).second);
      }
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
    for (const auto& [pid, _] : pid_to_udf) {
      if (::kill(pid, SIGKILL) == -1) {
        PLOG(ERROR) << "kill(" << pid << ", SIGKILL)";
      }
      if (::waitpid(pid, /*status=*/nullptr, /*options=*/0) == -1) {
        PLOG(ERROR) << "waitpid(" << pid << ", nullptr, 0)";
      }
    }
  };
  FileInputStream input(fd);
  while (true) {
    DispatcherRequest request;
    if (!ParseDelimitedFromZeroCopyStream(&request, &input, nullptr)) {
      break;
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
    {
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
    }
    for (int i = 0; i < request.load_binary().num_workers() - 1; ++i) {
      const std::optional<int> pid = ConnectSendCloneAndExec(
          socket_name, request.load_binary().code_token(),
          binary_path.native());
      if (!pid.has_value()) {
        return -1;
      }
      UdfInstanceMetadata udf{
          .code_token = request.load_binary().code_token(),
          .binary_path = binary_path,
      };
      absl::MutexLock lock(&mu);
      pid_to_udf[*pid] = std::move(udf);
    }

    // Start n-th worker out of loop.
    const std::optional<int> pid = ConnectSendCloneAndExec(
        socket_name, request.load_binary().code_token(), binary_path.native());
    if (!pid.has_value()) {
      return -1;
    }
    UdfInstanceMetadata udf{
        .code_token =
            std::move(*request.mutable_load_binary()->mutable_code_token()),
        .binary_path = binary_path,
    };
    absl::MutexLock lock(&mu);
    pid_to_udf[*pid] = std::move(udf);
  }
  return 0;
}
