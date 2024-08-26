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

#include "dispatcher.h"

#include <limits.h>
#include <sys/socket.h>
#include <unistd.h>

#include <queue>
#include <string_view>
#include <thread>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/functional/function_ref.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/core/common/uuid/uuid.h"
#include "src/roma/byob/dispatcher/dispatcher.pb.h"
#include "src/roma/byob/host/callback.pb.h"

namespace privacy_sandbox::server_common::byob {
namespace {
using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using ::google::protobuf::util::SerializeDelimitedToFileDescriptor;
using ::google::scp::core::common::Uuid;
using ::google::scp::roma::proto::FunctionBindingIoProto;
}  // namespace

Dispatcher::~Dispatcher() {
  {
    absl::MutexLock lock(&mu_);
    mu_.Await(absl::Condition(
        +[](int* i) { return *i == 0; }, &executor_threads_in_flight_));
  }
  ::close(connection_fd_);
  ::shutdown(listen_fd_, SHUT_RDWR);
  if (acceptor_.has_value()) {
    acceptor_->join();
  }
  for (auto& [_, fds] : code_token_to_fds_) {
    while (!fds.empty()) {
      ::close(fds.front());
      fds.pop();
    }
  }
  ::close(listen_fd_);
}

absl::Status Dispatcher::Init(const int listen_fd) {
  listen_fd_ = listen_fd;
  connection_fd_ = ::accept(listen_fd_, nullptr, nullptr);
  if (connection_fd_ == -1) {
    return absl::InternalError(
        absl::StrCat("Failed to accept on fd=", listen_fd_));
  }
  acceptor_.emplace(&Dispatcher::AcceptorImpl, this);
  return absl::OkStatus();
}

std::string Dispatcher::LoadBinary(std::string_view binary_path,
                                   const int n_workers) {
  std::string code_token = ToString(Uuid::GenerateUuid());
  LoadRequest payload;
  payload.set_binary_path(binary_path);
  payload.set_code_token(code_token);
  payload.set_n_workers(n_workers);
  SerializeDelimitedToFileDescriptor(payload, connection_fd_);
  return code_token;
}

void Dispatcher::AcceptorImpl() {
  while (true) {
    const int fd = ::accept(listen_fd_, nullptr, nullptr);
    if (fd == -1) {
      break;
    }
    char buffer[37];

    // Initialize buffer with zeros.
    ::memset(buffer, 0, sizeof(buffer));
    (void)::read(fd, buffer, 36);
    absl::MutexLock lock(&mu_);
    code_token_to_fds_[buffer].push(fd);
  }
}

namespace {
void RunCallback(
    Callback callback,
    absl::FunctionRef<void(std::string_view, FunctionBindingIoProto&)> handler,
    int fd, absl::Mutex* mu, int* outstanding_threads) {
  handler(callback.function_name(), *callback.mutable_io_proto());
  absl::MutexLock lock(mu);
  SerializeDelimitedToFileDescriptor(callback, fd);
  --(*outstanding_threads);
}
}  // namespace

void Dispatcher::ExecutorImpl(
    std::string_view code_token, google::protobuf::Any request,
    absl::AnyInvocable<void(absl::StatusOr<std::string>) &&> callback,
    absl::FunctionRef<void(std::string_view, FunctionBindingIoProto&)>
        handler) {
  int fd;
  {
    auto fn = [&] {
      mu_.AssertReaderHeld();
      return !code_token_to_fds_[code_token].empty();
    };
    absl::MutexLock l(&mu_);
    mu_.Await(absl::Condition(&fn));
    auto& fds = code_token_to_fds_[code_token];
    fd = fds.front();
    fds.pop();
  }
  SerializeDelimitedToFileDescriptor(request, fd);
  FileInputStream input(fd);
  absl::Mutex mu;
  int outstanding_threads = 0;  // Guarded by mu.
  while (true) {
    google::protobuf::Any any;
    ParseDelimitedFromZeroCopyStream(&any, &input, nullptr);
    if (any.Is<Callback>()) {
      {
        absl::MutexLock lock(&mu);
        ++outstanding_threads;
      }
      Callback callback;
      CHECK(any.UnpackTo(&callback));
      std::thread(RunCallback, std::move(callback), handler, fd, &mu,
                  &outstanding_threads)
          .detach();
    } else {
      std::move(callback)(std::move(*any.mutable_value()));
      break;
    }
  }
  {
    absl::MutexLock lock(&mu);
    mu.Await(
        absl::Condition(+[](int* i) { return *i == 0; }, &outstanding_threads));
  }
  ::close(fd);
  absl::MutexLock lock(&mu_);
  --executor_threads_in_flight_;
}
}  // namespace privacy_sandbox::server_common::byob
