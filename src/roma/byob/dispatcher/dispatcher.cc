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

absl::StatusOr<std::string> Read(int fd, int size) {
  std::string buffer(size, '\0');
  size_t read_bytes = 0;
  while (read_bytes < size) {
    const ssize_t n = ::read(fd, &buffer[read_bytes], size - read_bytes);
    if (n == -1) {
      return absl::ErrnoToStatus(errno, "Failed to read data from fd.");
    } else if (n == 0) {
      return absl::UnavailableError("Unexpected EOF.");
    }
    read_bytes += n;
  }
  return buffer;
}

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

absl::Status Dispatcher::Init(const int listen_fd,
                              std::filesystem::path log_dir) {
  listen_fd_ = listen_fd;
  log_dir_ = log_dir;
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
    std::filesystem::path binary_path, const int num_workers,
    const bool enable_log_egress) {
  if (num_workers <= 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("`num_workers=", num_workers, "` must be positive"));
  }
  std::string code_token = ToString(Uuid::GenerateUuid());
  DispatcherRequest request;
  auto& payload = *request.mutable_load_binary();
  std::error_code ec;
  if (std::filesystem::file_status fstatus =
          std::filesystem::status(binary_path, ec);
      ec) {
    return absl::PermissionDeniedError(absl::StrCat(
        "error accessing file ", binary_path.string(), ": ", ec.message()));
  } else if (fstatus.type() != std::filesystem::file_type::regular) {
    return absl::InternalError(
        absl::StrCat("unexpected file type for ", binary_path.string()));
  }
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
  payload.set_enable_log_egress(enable_log_egress);
  {
    absl::MutexLock lock(&mu_);
    code_token_to_fds_and_tokens_.insert({code_token, {}});
  }
  SerializeDelimitedToFileDescriptor(request, connection_fd_);
  return code_token;
}

absl::StatusOr<std::string> Dispatcher::LoadBinaryForLogging(
    std::string source_bin_code_token, int num_workers) {
  if (num_workers <= 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("`num_workers=", num_workers, "` must be positive"));
  }
  std::string code_token = ToString(Uuid::GenerateUuid());
  DispatcherRequest request;
  auto& payload = *request.mutable_load_binary();
  payload.set_code_token(code_token);
  payload.set_num_workers(num_workers);
  payload.set_source_bin_code_token(source_bin_code_token);
  payload.set_enable_log_egress(true);
  {
    absl::MutexLock lock(&mu_);
    code_token_to_fds_and_tokens_.insert({code_token, {}});
  }
  SerializeDelimitedToFileDescriptor(request, connection_fd_);
  return code_token;
}

void Dispatcher::Delete(std::string_view code_token) {
  DispatcherRequest request;
  request.mutable_delete_binary()->set_code_token(code_token);
  SerializeDelimitedToFileDescriptor(request, connection_fd_);
  absl::MutexLock lock(&mu_);
  if (const auto it = code_token_to_fds_and_tokens_.find(code_token);
      it != code_token_to_fds_and_tokens_.end()) {
    while (!it->second.empty()) {
      ::close(it->second.front().fd);
      it->second.pop();
    }
    code_token_to_fds_and_tokens_.erase(it);
  }
}

void Dispatcher::Cancel(google::scp::roma::ExecutionToken execution_token) {
  DispatcherRequest request;
  request.mutable_cancel()->set_execution_token(
      std::move(execution_token).value);
  SerializeDelimitedToFileDescriptor(request, connection_fd_);
}

void Dispatcher::AcceptorImpl() {
  absl::Mutex thread_count_mu;
  int thread_count = 0;  // Guarded by `thread_count_mu`.
  auto read_tokens_and_push_to_queue = [this, &thread_count_mu,
                                        &thread_count](int fd) {
    // Read code token and execution token, 36 bytes each.
    // First is code token, second is execution token.
    if (absl::StatusOr<std::string> data = Read(fd, kNumTokenBytes * 2);
        !data.ok()) {
      LOG(ERROR) << "Read failure: " << data.status();
      ::close(fd);
    } else {
      std::string execution_token = data->substr(kNumTokenBytes);
      data->resize(kNumTokenBytes);
      absl::MutexLock lock(&mu_);
      if (const auto it = code_token_to_fds_and_tokens_.find(*data);
          it != code_token_to_fds_and_tokens_.end()) {
        it->second.push(FdAndToken{
            .fd = fd,
            .token = std::move(execution_token),
        });
      } else {
        LOG(ERROR) << "Unrecognized code token.";
        ::close(fd);
      }
    }
    absl::MutexLock lock(&thread_count_mu);
    --thread_count;
  };
  while (true) {
    const int fd = ::accept(listen_fd_, nullptr, nullptr);
    if (fd == -1) {
      break;
    }
    {
      absl::MutexLock lock(&thread_count_mu);
      ++thread_count;
    }
    std::thread(read_tokens_and_push_to_queue, fd).detach();
  }
  absl::MutexLock lock(&thread_count_mu);
  thread_count_mu.Await(
      absl::Condition(+[](int* i) { return *i == 0; }, &thread_count));
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
