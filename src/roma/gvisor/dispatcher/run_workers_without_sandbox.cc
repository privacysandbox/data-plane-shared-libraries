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

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

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
#include "src/roma/gvisor/dispatcher/dispatcher.pb.h"

ABSL_FLAG(std::string, socket_name, "/sockdir/abcd.sock",
          "Server socket for reaching Roma app API");

namespace {
using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using ::privacy_sandbox::server_common::gvisor::LoadRequest;

bool ConnectToPath(int fd, std::string_view socket_name) {
  sockaddr_un sa;
  ::memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  ::strncpy(sa.sun_path, socket_name.data(), sizeof(sa.sun_path));
  return ::connect(fd, reinterpret_cast<sockaddr*>(&sa), SUN_LEN(&sa)) == 0;
}
struct WorkerImplArg {
  int io_fd;
  int callback_fd;
  std::string_view code_token;
  std::string_view binary_path;
};

int WorkerImpl(void* arg) {
  const WorkerImplArg& worker_impl_arg = *static_cast<WorkerImplArg*>(arg);
  PCHECK(::write(worker_impl_arg.io_fd, worker_impl_arg.code_token.data(),
                 worker_impl_arg.code_token.size()) ==
         worker_impl_arg.code_token.size());

  // Exec binary.
  PCHECK(::dup2(worker_impl_arg.io_fd, STDIN_FILENO) != -1);
  const std::string io_fd = [=] {
    const int io_fd = ::dup(worker_impl_arg.io_fd);
    PCHECK(io_fd != -1);
    return absl::StrCat(io_fd);
  }();
  const std::string callback_fd = [=] {
    const int callback_fd = ::dup(worker_impl_arg.callback_fd);
    PCHECK(callback_fd != -1);
    return absl::StrCat(callback_fd);
  }();
  ::execl(worker_impl_arg.binary_path.data(),
          worker_impl_arg.binary_path.data(), io_fd.c_str(),
          callback_fd.c_str(), nullptr);
  PLOG(FATAL) << "execl() failed";
}
bool ConnectAndExec(std::string_view socket_name, std::string_view code_token,
                    std::string_view binary_path, int* output_pid) {
  const int io_fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  PCHECK(io_fd != -1);
  const int callback_fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  PCHECK(callback_fd != -1);
  if (!ConnectToPath(io_fd, socket_name) ||
      !ConnectToPath(callback_fd, socket_name)) {
    PLOG(INFO) << "connect() to " << socket_name << "failed";
    return false;
  }
  WorkerImplArg worker_impl_arg{io_fd, callback_fd, code_token, binary_path};
  char stack[1 << 20];
  const pid_t pid = ::clone(WorkerImpl, stack + sizeof(stack),
                            CLONE_VM | CLONE_VFORK | SIGCHLD, &worker_impl_arg);
  PCHECK(::close(io_fd) == 0);
  PCHECK(::close(callback_fd) == 0);
  PCHECK(pid != -1);
  *output_pid = pid;
  return true;
}
}  // namespace

int main(int argc, char** argv) {
  std::vector<char*> args = absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  const std::string socket_name = absl::GetFlag(FLAGS_socket_name);
  const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  PCHECK(fd != -1);
  PCHECK(ConnectToPath(fd, socket_name));
  struct UdfInstanceMetadata {
    std::string code_token;
    std::string binary_path;
  };

  absl::Mutex mu;
  absl::flat_hash_map<int, UdfInstanceMetadata> pid_to_udf;  // Guarded by mu.
  bool shutdown = false;                                     // Guarded by mu.
  std::thread reloader([&socket_name, &mu, &pid_to_udf, &shutdown] {
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
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
      }

      // Start a new worker with the same UDF as the most-recently ended worker.
      absl::MutexLock lock(&mu);
      int pid;
      if (!ConnectAndExec(socket_name, udf.code_token, udf.binary_path, &pid)) {
        return;
      }
      CHECK(pid_to_udf.insert({pid, std::move(udf)}).second);
    }
  });
  absl::Cleanup cleanup = [&] {
    {
      absl::MutexLock lock(&mu);
      shutdown = true;
    }
    reloader.join();
    ::close(fd);

    // Kill extant workers before exit.
    for (const auto& [pid, _] : pid_to_udf) {
      PCHECK(::kill(pid, SIGKILL) == 0);
      PCHECK(::waitpid(pid, nullptr, /*options=*/0) != -1);
    }
  };
  FileInputStream input(fd);
  while (true) {
    LoadRequest request;
    if (!ParseDelimitedFromZeroCopyStream(&request, &input, nullptr)) {
      break;
    }
    for (int i = 0; i < request.n_workers() - 1; ++i) {
      absl::MutexLock lock(&mu);
      int pid;
      if (!ConnectAndExec(socket_name, request.code_token(),
                          request.binary_path(), &pid)) {
        return -1;
      }
      UdfInstanceMetadata udf{
          .code_token = request.code_token(),
          .binary_path = request.binary_path(),
      };
      pid_to_udf[pid] = std::move(udf);
    }

    // Start n-th worker out of loop.
    absl::MutexLock lock(&mu);
    int pid;
    if (!ConnectAndExec(socket_name, request.code_token(),
                        request.binary_path(), &pid)) {
      return -1;
    }
    UdfInstanceMetadata udf{
        .code_token = std::move(*request.mutable_code_token()),
        .binary_path = std::move(*request.mutable_binary_path()),
    };
    pid_to_udf[pid] = std::move(udf);
  }
  return 0;
}
