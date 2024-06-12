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

#include "src/roma/gvisor/container/sandbox/pool_manager.h"

#include <fcntl.h> /* Definition of O_* constants */
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/util/status_macro/status_macros.h"

namespace privacy_sandbox::server_common::gvisor {

namespace {

constexpr int32_t kStackSize = 1 << 20;
constexpr int32_t kBufferChunkSize = 64 << 10;
constexpr absl::Duration kWorkerFetchTimeout = absl::Seconds(5);

absl::Status MkDir(std::string_view dir) {
  if (std::error_code ec;
      !std::filesystem::create_directories(dir.data(), ec)) {
    return absl::InternalError(
        absl::StrCat("Failed to mkdir ", dir, ": ", ec.message()));
  }
  return absl::OkStatus();
}

absl::Status MountToDir(std::string_view dir,
                        absl::Span<const std::string> mounts) {
  // Mount everything we need into the directory
  for (const std::string& mount_path : mounts) {
    const std::string dest_path = absl::StrCat(dir, mount_path);
    PS_RETURN_IF_ERROR(MkDir(dest_path));
    if (::mount(mount_path.c_str(), dest_path.c_str(), nullptr, MS_BIND,
                nullptr) < 0) {
      return absl::ErrnoToStatus(
          errno,
          absl::StrCat("Failed to mount ", mount_path, " to ", dest_path));
    }
  }
  return absl::OkStatus();
}

absl::Status RemountForPivot(std::string_view path) {
  if (::mount(path.data(), path.data(), nullptr, MS_REMOUNT | MS_BIND,
              nullptr) < 0) {
    return absl::ErrnoToStatus(errno, absl::StrCat("Failed to remount ", path));
  }
  return absl::OkStatus();
}

absl::Status RemountForPivot(absl::Span<const std::string> mounts) {
  for (const auto& mount : mounts) {
    PS_RETURN_IF_ERROR(RemountForPivot(mount));
  }
  return absl::OkStatus();
}

absl::Status SetupPivotRoot(std::string_view dir,
                            absl::Span<const std::string> mounts,
                            std::string_view prog_path) {
  // MS_REC needed here to get other mounts (/lib, /lib64 etc)
  if (::mount(dir.data(), dir.data(), "bind", MS_REC | MS_BIND, nullptr) < 0) {
    return absl::ErrnoToStatus(
        errno, absl::StrCat("Failed to mount MS_REC | MS_BIND", dir));
  }
  if (::mount(dir.data(), dir.data(), "bind", MS_REC | MS_SLAVE, nullptr) < 0) {
    return absl::ErrnoToStatus(
        errno, absl::StrCat("Failed to mount MS_REC | MS_SLAVE", dir));
  }
  const std::string pivot_dir = absl::StrCat(dir, "/pivot");
  PS_RETURN_IF_ERROR(MkDir(pivot_dir));

  if (::syscall(SYS_pivot_root, dir.data(), pivot_dir.data()) < 0) {
    return absl::ErrnoToStatus(errno, "Failed to call pivot_root");
  }
  if (::chdir("/") < 0) {
    return absl::ErrnoToStatus(errno, "Failed to chdir");
  }
  if (::umount2("/pivot", MNT_DETACH) < 0) {
    return absl::ErrnoToStatus(errno, "Failed to call umount2");
  }
  PS_RETURN_IF_ERROR(RemountForPivot(mounts));
  PS_RETURN_IF_ERROR(
      RemountForPivot(std::filesystem::path(prog_path).parent_path().c_str()));
  return absl::OkStatus();
}

struct WorkerArgs {
  int* request_pipe;
  std::string_view prog_path;
  absl::Span<const std::string> mounts;
  std::string_view pivot_root_dir;
  int* response_pipe;
  int comms_fd;
};

int RunWorker(void* worker_arg) {
  WorkerArgs* worker_args = static_cast<WorkerArgs*>(worker_arg);
  // Close the read end of the response pipe
  PCHECK(::close(worker_args->response_pipe[0]) == 0)
      << "Failed to close read-end of response_pipe "
      << worker_args->response_pipe[0] << " in worker.";
  // Close the write end of the response pipe
  PCHECK(::close(worker_args->request_pipe[1]) == 0)
      << "Failed to close write-end of request_pipe "
      << worker_args->request_pipe[1] << " in worker.";

  // Set up restricted filesystem for worker using pivot_root
  // pivot_root doesn't work under an MS_SHARED mount point.
  // https://man7.org/linux/man-pages/man2/pivot_root.2.html.
  PCHECK(::mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) == 0)
      << "Failed to mount /";
  CHECK_OK(MountToDir(worker_args->pivot_root_dir, worker_args->mounts));
  CHECK_OK(SetupPivotRoot(worker_args->pivot_root_dir, worker_args->mounts,
                          worker_args->prog_path));
  PCHECK(::dup2(worker_args->request_pipe[0], STDIN_FILENO) > -1)
      << "Failed to dup2 request pipe";
  const std::string response_pipe = [worker_args] {
    const int response_pipe = ::dup(worker_args->response_pipe[1]);
    PCHECK(response_pipe > -1) << "Failed to dup reponse pipe";
    return absl::StrCat(response_pipe);
  }();
  const std::string comms_fd = [worker_args] {
    const int comms_fd = ::dup(worker_args->comms_fd);
    PCHECK(comms_fd > -1) << "Failed to dup comms fd";
    return absl::StrCat(comms_fd);
  }();
  const std::vector<const char*> argv = {worker_args->prog_path.data(),
                                         response_pipe.c_str(),
                                         comms_fd.c_str(), nullptr};
  ::execve(worker_args->prog_path.data(), const_cast<char* const*>(argv.data()),
           nullptr);
  PLOG(ERROR) << "Failed to run '" << absl::StrJoin(argv, " ") << "'";
  abort();
}

absl::StatusOr<absl::Cord> ReadResponseFromPipe(const int pipe_fd,
                                                const int bytes_available) {
  absl::Cord response_cord;
  // Read from the pipe until there is no more data.
  int bytes_to_be_read = bytes_available;
  while (bytes_to_be_read > 0) {
    absl::CordBuffer buffer = absl::CordBuffer::CreateWithCustomLimit(
        kBufferChunkSize, bytes_to_be_read);
    absl::Span<char> data = buffer.available_up_to(bytes_to_be_read);
    int bytes_read = ::read(pipe_fd, data.data(), data.size());
    if (bytes_read < 0) {
      return absl::ErrnoToStatus(errno, "Failed to read from pipe");
    }
    buffer.IncreaseLengthBy(data.size());
    response_cord.Append(std::move(buffer));
    bytes_to_be_read -= data.size();
  }
  if (::close(pipe_fd) < 0) {
    return absl::ErrnoToStatus(
        errno, absl::StrCat("Failed to close pipe ", pipe_fd, " post-read."));
  }
  return response_cord;
}

absl::Status ClearWorkerQueue(std::queue<WorkerInfo>& worker_queue) {
  while (!worker_queue.empty()) {
    WorkerInfo worker_info = std::move(worker_queue.front());
    worker_queue.pop();
    if (::kill(worker_info.pid, SIGKILL) < 0) {
      return absl::ErrnoToStatus(
          errno, absl::StrCat("Failed to kill worker ", worker_info.pid,
                              " in destructor."));
    }
    if (std::error_code ec;
        std::filesystem::remove_all(worker_info.pivot_root_dir, ec) < 0) {
      return absl::InternalError(
          absl::StrCat("Failed to remove pivot root directory ",
                       worker_info.pivot_root_dir, ": ", ec.message()));
    }

    if (::close(worker_info.out_pipe) < 0) {
      return absl::ErrnoToStatus(
          errno, absl::StrCat("Failed to close out_pipe in destructor ",
                              worker_info.out_pipe));
    }
    if (::close(worker_info.in_pipe) < 0) {
      return absl::ErrnoToStatus(
          errno, absl::StrCat("Failed to close in_pipe in destructor ",
                              worker_info.in_pipe));
    }
    if (::close(worker_info.comms_fd) < 0) {
      return absl::ErrnoToStatus(
          errno, absl::StrCat("Failed to close comms socket ",
                              worker_info.comms_fd, " on parent."));
    }
  }
  return absl::OkStatus();
}
};  // namespace

RomaGvisorPoolManager::RomaGvisorPoolManager(
    int worker_pool_size, absl::Span<const std::string> mounts,
    std::string_view prog_dir, std::string_view callback_socket)
    : worker_pool_size_(worker_pool_size),
      mounts_(mounts),
      prog_dir_(prog_dir),
      callback_socket_(callback_socket) {}

RomaGvisorPoolManager::~RomaGvisorPoolManager() {
  if (absl::Status status = ClearWorkerMap(); !status.ok()) {
    LOG(ERROR) << "Failed to clear worker queue: " << status;
  }
}

absl::Status RomaGvisorPoolManager::ClearWorkerMap() {
  absl::MutexLock lock(&worker_map_mu_);
  for (auto& [code_token, worker_queue] : worker_map_) {
    PS_RETURN_IF_ERROR(ClearWorkerQueue(worker_queue));
  }
  return absl::OkStatus();
}

absl::StatusOr<std::string> RomaGvisorPoolManager::LoadBinary(
    std::string_view code_token, std::string_view code) {
  PS_RETURN_IF_ERROR(ClearWorkerMap());
  std::filesystem::path prog_path =
      std::filesystem::path(prog_dir_) / std::filesystem::path(code_token);
  std::ofstream ofs(prog_path.c_str(), std::ofstream::trunc);
  std::filesystem::permissions(prog_path.c_str(),
                               std::filesystem::perms::owner_all);
  if (!ofs.is_open() || ofs.fail()) {
    return absl::InternalError("Failed to open file");
  }
  ofs << code;
  ofs.close();
  PS_RETURN_IF_ERROR(
      PopulateWorkerQueue(code_token, prog_path.c_str(), worker_pool_size_));
  return std::string(code_token);
}

namespace {
int ConnectToNamedSocketOrDie(std::string_view name) {
  sockaddr_un sa;
  ::memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  ::strncpy(sa.sun_path, name.data(), sizeof(sa.sun_path));
  const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  PCHECK(fd != -1);
  PCHECK(::connect(fd, reinterpret_cast<sockaddr*>(&sa), SUN_LEN(&sa)) == 0);
  return fd;
}
}  // namespace

absl::StatusOr<WorkerInfo> RomaGvisorPoolManager::CreateAndRunWorker(
    std::string_view prog_path) {
  char tmp_file[] = "/tmp/roma_app_server_XXXXXX";
  char* pivot_root_dir = ::mkdtemp(tmp_file);
  int response_pipe[2];
  int request_pipe[2];
  if (::pipe2(request_pipe, O_CLOEXEC) < 0) {
    return absl::ErrnoToStatus(errno, "Failed to create request pipe");
  }
  if (::pipe2(response_pipe, O_CLOEXEC) < 0) {
    return absl::ErrnoToStatus(errno, "Failed to create response pipe");
  }
  const int comms_fd = ConnectToNamedSocketOrDie(callback_socket_);
  WorkerArgs worker_args = {
      &request_pipe[0], prog_path,         mounts_,
      pivot_root_dir,   &response_pipe[0], comms_fd,
  };
  char stack[kStackSize];
  pid_t pid = ::clone(RunWorker, stack + kStackSize,
                      CLONE_VM | CLONE_VFORK | CLONE_NEWIPC | CLONE_NEWPID |
                          SIGCHLD | CLONE_NEWUTS | CLONE_NEWNS,
                      &worker_args);
  if (pid < 0) {
    return absl::ErrnoToStatus(errno, "Failed to clone a worker");
  }
  if (::close(response_pipe[1]) < 0) {
    return absl::ErrnoToStatus(
        errno, absl::StrCat("Failed to close write-end of response_pipe ",
                            response_pipe[1], " on parent."));
  }
  if (::close(request_pipe[0]) < 0) {
    return absl::ErrnoToStatus(
        errno, absl::StrCat("Failed to close read-end of request_pipe ",
                            request_pipe[0], " on parent."));
  }
  return WorkerInfo{
      .pid = pid,
      .in_pipe = request_pipe[1],
      .out_pipe = response_pipe[0],
      .pivot_root_dir = std::move(pivot_root_dir),
      .comms_fd = comms_fd,
  };
}

absl::Status RomaGvisorPoolManager::PopulateWorkerQueue(
    std::string_view code_token, std::string_view prog_path,
    const int num_workers) {
  if (num_workers <= 0) {
    return absl::OkStatus();
  }
  int pool_size = num_workers;
  while (pool_size--) {
    PS_ASSIGN_OR_RETURN(WorkerInfo worker_info, CreateAndRunWorker(prog_path));
    absl::MutexLock lock(&worker_map_mu_);
    auto worker_map_it = worker_map_.find(code_token);
    if (worker_map_it == worker_map_.end()) {
      std::queue<WorkerInfo> worker_queue;
      worker_queue.push(std::move(worker_info));
      worker_map_.try_emplace(code_token, std::move(worker_queue));
    } else {
      worker_map_[code_token].push(std::move(worker_info));
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<WorkerInfo> RomaGvisorPoolManager::GetWorker(
    std::string_view code_token) {
  std::filesystem::path prog_path =
      std::filesystem::path(prog_dir_) / std::filesystem::path(code_token);
  if (std::error_code ec; !std::filesystem::exists(prog_path, ec)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Code file for code_token ", code_token, " not found: ", ec.message()));
  }
  // Before utilizing a worker, preempt creation of a replacement.
  std::thread([&, code_token = std::string(code_token),
               prog_path = prog_path]() {
    CHECK_OK(PopulateWorkerQueue(code_token, prog_path.c_str(), 1));
  }).detach();
  absl::MutexLock lock(&worker_map_mu_);
  auto fn = [this, &code_token] {
    worker_map_mu_.AssertReaderHeld();
    auto worker_queue_it = worker_map_.find(code_token);
    if (worker_queue_it == worker_map_.end()) {
      return false;
    }
    return !worker_queue_it->second.empty();
  };
  if (!worker_map_mu_.AwaitWithTimeout(absl::Condition(&fn),
                                       kWorkerFetchTimeout)) {
    return absl::DeadlineExceededError(
        "Could not acquire a worker within the timeout");
  }
  WorkerInfo worker_info = std::move(worker_map_[code_token].front());
  worker_map_[code_token].pop();
  return worker_info;
}

absl::StatusOr<absl::Cord>
RomaGvisorPoolManager::SendRequestAndGetResponseFromWorker(
    std::string_view request_id, std::string_view code_token,
    std::string_view serialized_bin_request) {
  if (code_token.empty()) {
    return absl::InvalidArgumentError("Expected non-empty code token");
  }
  PS_ASSIGN_OR_RETURN(const WorkerInfo worker_info, GetWorker(code_token));

  Uuid uuid;
  // Write the request_id to the callback pipe. The request_id is not passed to
  // the worker.
  uuid.set_uuid(request_id);
  if (!google::protobuf::util::SerializeDelimitedToFileDescriptor(
          uuid, worker_info.comms_fd)) {
    return absl::InternalError("Failed to send uuid to callback server.");
  }
  if (::close(worker_info.comms_fd) < 0) {
    return absl::ErrnoToStatus(
        errno, absl::StrCat("Failed to close comms socket ",
                            worker_info.comms_fd, " on parent."));
  }
  if (::write(worker_info.in_pipe, serialized_bin_request.data(),
              serialized_bin_request.size()) < 0) {
    return absl::ErrnoToStatus(errno, "Failed to write to worker");
  }
  if (::close(worker_info.in_pipe) < 0) {
    return absl::ErrnoToStatus(
        errno, absl::StrCat("Failed to close in_pipe ", worker_info.in_pipe,
                            " post-write"));
  }

  int status;
  if (::waitpid(worker_info.pid, &status, 0) < 0) {
    return absl::ErrnoToStatus(
        errno, absl::StrCat("Failed waitpid call worker ", worker_info.pid,
                            " to complete"));
  }
  std::filesystem::remove_all(worker_info.pivot_root_dir);
  if (!WIFEXITED(status)) {
    PCHECK(::close(worker_info.out_pipe) == 0)
        << "Failed to close out_pipe " << worker_info.out_pipe
        << " when worker did not exit normally";
    return absl::InternalError("Worker process did not exit normally.");
  } else if (int worker_errno = WEXITSTATUS(status);
             worker_errno != EXIT_SUCCESS) {
    PCHECK(::close(worker_info.out_pipe) == 0)
        << "Failed to close out_pipe " << worker_info.out_pipe
        << " when worker did not exit with non-zero code.";
    return absl::ErrnoToStatus(
        worker_errno,
        absl::StrCat("Worker process did not exited with non-zero code ",
                     worker_errno));
  }
  int bytes_available = 0;
  if (::ioctl(worker_info.out_pipe, FIONREAD, &bytes_available) < 0) {
    return absl::ErrnoToStatus(errno, "Failed to find output size");
  }
  if (bytes_available < 0) {
    return absl::InvalidArgumentError(absl::StrCat(
        "No response written to output pipe ", worker_info.out_pipe));
  }
  return ReadResponseFromPipe(worker_info.out_pipe, bytes_available);
}
}  // namespace privacy_sandbox::server_common::gvisor
