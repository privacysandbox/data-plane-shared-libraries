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
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <fstream>
#include <queue>
#include <string>
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
#include "src/util/execution_token.h"

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
  for (auto& [_, fds_and_tokens] : code_token_to_fds_and_tokens_) {
    while (!fds_and_tokens.empty()) {
      ::close(fds_and_tokens.front().fd);
      fds_and_tokens.pop();
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

  // Ignore SIGPIPE. Otherwise, host process will crash when UDFs close sockets
  // before or while Roma writes requests or callback responses.
  ::signal(SIGPIPE, SIG_IGN);
  acceptor_.emplace(&Dispatcher::AcceptorImpl, this);
  return absl::OkStatus();
}

absl::StatusOr<std::string> Dispatcher::LoadBinary(
    std::filesystem::path binary_path, const int num_workers) {
  if (num_workers <= 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("`num_workers=", num_workers, "` must be positive"));
  }
  std::string code_token = ToString(Uuid::GenerateUuid());
  DispatcherRequest request;
  auto& payload = *request.mutable_load_binary();
  if (std::ifstream ifs(std::move(binary_path), std::ios::binary);
      ifs.is_open()) {
    payload.set_binary_content(std::string(std::istreambuf_iterator<char>(ifs),
                                           std::istreambuf_iterator<char>()));
  } else {
    return absl::UnavailableError(
        absl::StrCat("Cannot open ", binary_path.native()));
  }
  payload.set_code_token(code_token);
  payload.set_num_workers(num_workers);
  SerializeDelimitedToFileDescriptor(request, connection_fd_);
  return code_token;
}

void Dispatcher::Delete(std::string_view code_token) {
  DispatcherRequest request;
  request.mutable_delete_binary()->set_code_token(code_token);
  SerializeDelimitedToFileDescriptor(request, connection_fd_);
}

void Dispatcher::Cancel(google::scp::roma::ExecutionToken execution_token) {
  DispatcherRequest request;
  request.mutable_cancel()->set_execution_token(
      std::move(execution_token).value);
  SerializeDelimitedToFileDescriptor(request, connection_fd_);
}

void Dispatcher::AcceptorImpl() {
  while (true) {
    const int fd = ::accept(listen_fd_, nullptr, nullptr);
    if (fd == -1) {
      break;
    }
    char code_token[37] = {};
    char execution_token[37] = {};
    (void)::read(fd, code_token, 36);
    (void)::read(fd, execution_token, 36);
    absl::MutexLock lock(&mu_);
    code_token_to_fds_and_tokens_[code_token].push(FdAndToken{
        .fd = fd,
        .token = execution_token,
    });
  }
}

namespace {
void RunCallback(
    Callback callback,
    absl::FunctionRef<void(std::string_view, FunctionBindingIoProto&)> handler,
    int fd, absl::Mutex* mu, int* outstanding_threads) {
  handler(callback.function_name(), *callback.mutable_io_proto());
  SerializeDelimitedToFileDescriptor(callback, fd);
  absl::MutexLock lock(mu);
  --(*outstanding_threads);
}
}  // namespace

void Dispatcher::ExecutorImpl(
    const int fd, google::protobuf::Any request,
    absl::AnyInvocable<void(absl::StatusOr<google::protobuf::Any>) &&> callback,
    absl::FunctionRef<void(std::string_view, FunctionBindingIoProto&)>
        handler) {
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
      std::move(callback)(std::move(any));
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
