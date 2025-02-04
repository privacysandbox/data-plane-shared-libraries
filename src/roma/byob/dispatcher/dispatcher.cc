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
#include <sys/un.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "absl/functional/any_invocable.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/core/common/uuid/uuid.h"
#include "src/roma/byob/dispatcher/dispatcher.grpc.pb.h"
#include "src/roma/byob/dispatcher/dispatcher.pb.h"
#include "src/roma/byob/dispatcher/interface.h"
#include "src/util/execution_token.h"
#include "src/util/status_macro/status_util.h"

namespace privacy_sandbox::server_common::byob {
namespace {
using ::google::protobuf::util::SerializeDelimitedToFileDescriptor;
using ::google::scp::core::common::Uuid;
using ::privacy_sandbox::server_common::byob::CancelRequest;
using ::privacy_sandbox::server_common::byob::CancelResponse;
using ::privacy_sandbox::server_common::byob::DeleteBinaryRequest;
using ::privacy_sandbox::server_common::byob::DeleteBinaryResponse;
using ::privacy_sandbox::server_common::byob::LoadBinaryRequest;
using ::privacy_sandbox::server_common::byob::LoadBinaryResponse;

constexpr absl::Duration kWorkerCreationTimeout = absl::Seconds(10);

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
  ::shutdown(listen_fd_, SHUT_RDWR);
  {
    absl::MutexLock lock(&mu_);
    for (auto& [_, request_metadatas] : code_token_to_request_metadatas_) {
      while (!request_metadatas.empty()) {
        request_metadatas.front()->ready.Notify();
        request_metadatas.pop();
      }
    }
    code_token_to_request_metadatas_.clear();
    mu_.Await(absl::Condition(
        +[](int* i) { return *i == 0; }, &acceptor_threads_in_flight_));
  }
  ::close(listen_fd_);
}

absl::Status Dispatcher::Init(std::filesystem::path control_socket_name,
                              std::filesystem::path udf_socket_name,
                              std::filesystem::path log_dir) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd == -1) {
    return absl::ErrnoToStatus(errno, "socket()");
  }
  {
    ::sockaddr_un sa = {
        .sun_family = AF_UNIX,
    };
    udf_socket_name.native().copy(sa.sun_path, sizeof(sa.sun_path));
    if (::bind(fd, reinterpret_cast<::sockaddr*>(&sa), SUN_LEN(&sa)) == -1) {
      ::close(fd);
      return absl::ErrnoToStatus(errno, "bind()");
    }
    if (::listen(fd, /*backlog=*/4096) == -1) {
      ::close(fd);
      return absl::ErrnoToStatus(errno, "listen()");
    }
  }
  listen_fd_ = fd;
  std::filesystem::permissions(udf_socket_name,
                               std::filesystem::perms::owner_read |
                                   std::filesystem::perms::group_read |
                                   std::filesystem::perms::others_read |
                                   std::filesystem::perms::owner_write |
                                   std::filesystem::perms::group_write |
                                   std::filesystem::perms::others_write);
  log_dir_ = std::move(log_dir);
  std::shared_ptr<grpc::Channel> channel =
      grpc::CreateChannel(absl::StrCat("unix:", control_socket_name.native()),
                          grpc::InsecureChannelCredentials());

  // Blocks for 5 minutes or until a connection is established.
  if (!channel->WaitForConnected(
          gpr_time_add(gpr_now(GPR_CLOCK_REALTIME),
                       gpr_time_from_seconds(300, GPR_TIMESPAN)))) {
    return absl::DeadlineExceededError("Failed to connect to worker runner.");
  }
  stub_ = WorkerRunnerService::NewStub(std::move(channel));

  // Ignore SIGPIPE. Otherwise, host process will crash when UDFs close sockets
  // before or while Roma writes requests or callback responses.
  ::signal(SIGPIPE, SIG_IGN);
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
  LoadBinaryRequest request;
  if (std::ifstream ifs(std::move(binary_path), std::ios::binary);
      ifs.is_open()) {
    request.set_binary_content(std::string(std::istreambuf_iterator<char>(ifs),
                                           std::istreambuf_iterator<char>()));
  } else {
    return absl::UnavailableError(
        absl::StrCat("Cannot open ", binary_path.native()));
  }
  request.set_code_token(code_token);
  request.set_num_workers(num_workers);
  request.set_enable_log_egress(enable_log_egress);
  {
    absl::MutexLock lock(&mu_);
    code_token_to_request_metadatas_.insert({code_token, {}});
  }
  grpc::ClientContext context;
  LoadBinaryResponse response;
  if (const grpc::Status status =
          stub_->LoadBinary(&context, request, &response);
      !status.ok()) {
    absl::MutexLock lock(&mu_);
    code_token_to_request_metadatas_.erase(code_token);
    return privacy_sandbox::server_common::ToAbslStatus(status);
  }
  {
    absl::MutexLock lock(&mu_);
    acceptor_threads_in_flight_ += num_workers;
  }
  for (int i = 0; i < num_workers; ++i) {
    std::thread(&Dispatcher::AcceptorImpl, this, code_token).detach();
  }
  bool workers_have_been_created = false;
  {
    auto fn = [&] {
      mu_.AssertReaderHeld();
      const auto it = code_token_to_request_metadatas_.find(code_token);
      return !it->second.empty();
    };
    absl::MutexLock l(&mu_);
    workers_have_been_created =
        mu_.AwaitWithTimeout(absl::Condition(&fn), kWorkerCreationTimeout);
  }
  if (!workers_have_been_created) {
    Delete(code_token);
    return absl::InternalError("Worker creation failure.");
  }
  return code_token;
}

absl::StatusOr<std::string> Dispatcher::LoadBinaryForLogging(
    std::string source_bin_code_token, int num_workers) {
  if (num_workers <= 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("`num_workers=", num_workers, "` must be positive"));
  }
  std::string code_token = ToString(Uuid::GenerateUuid());
  LoadBinaryRequest request;
  request.set_code_token(code_token);
  request.set_num_workers(num_workers);
  request.set_source_bin_code_token(source_bin_code_token);
  request.set_enable_log_egress(true);
  {
    absl::MutexLock lock(&mu_);
    code_token_to_request_metadatas_.insert({code_token, {}});
  }
  grpc::ClientContext context;
  LoadBinaryResponse response;
  if (const grpc::Status status =
          stub_->LoadBinary(&context, request, &response);
      !status.ok()) {
    absl::MutexLock lock(&mu_);
    code_token_to_request_metadatas_.erase(code_token);
    return privacy_sandbox::server_common::ToAbslStatus(status);
  }
  {
    absl::MutexLock lock(&mu_);
    acceptor_threads_in_flight_ += num_workers;
  }
  for (int i = 0; i < num_workers; ++i) {
    std::thread(&Dispatcher::AcceptorImpl, this, code_token).detach();
  }
  bool workers_have_been_created = false;
  {
    auto fn = [&] {
      mu_.AssertReaderHeld();
      const auto it = code_token_to_request_metadatas_.find(code_token);
      return !it->second.empty();
    };
    absl::MutexLock l(&mu_);
    workers_have_been_created =
        mu_.AwaitWithTimeout(absl::Condition(&fn), kWorkerCreationTimeout);
  }
  if (!workers_have_been_created) {
    Delete(code_token);
    return absl::InternalError("Worker creation failure.");
  }
  return code_token;
}

void Dispatcher::Delete(std::string_view code_token) {
  {
    grpc::ClientContext context;
    DeleteBinaryRequest request;
    request.set_code_token(code_token);
    DeleteBinaryResponse response;
    stub_->DeleteBinary(&context, request, &response);
  }
  absl::MutexLock lock(&mu_);
  if (const auto it = code_token_to_request_metadatas_.find(code_token);
      it != code_token_to_request_metadatas_.end()) {
    while (!it->second.empty()) {
      it->second.front()->ready.Notify();
      it->second.pop();
    }
    code_token_to_request_metadatas_.erase(it);
  }
}

void Dispatcher::Cancel(google::scp::roma::ExecutionToken execution_token) {
  grpc::ClientContext context;
  CancelRequest request;
  request.set_execution_token(execution_token.value);
  CancelResponse response;
  stub_->Cancel(&context, request, &response);
}

// `parent_code_token` identifies the code_token generated by the `Load` call
// that detached this thread. The loop breaks when the code_token is deleted,
// ensuring the number of acceptors scales back. Note that an acceptor will
// accept connections without regard to UDF code_token.
void Dispatcher::AcceptorImpl(std::string parent_code_token) {
  // Break loop if the code_token generated by the `Load` call that detached
  // this thread was deleted.
  bool parent_code_token_deleted = false;
  while (!parent_code_token_deleted) {
    const int fd = ::accept(listen_fd_, nullptr, nullptr);
    if (fd == -1) {
      break;
    }

    // Read code token and execution token, 36 bytes each.
    // First is code token, second is execution token.
    absl::StatusOr<std::string> data = Read(fd, kNumTokenBytes * 2);
    if (!data.ok()) {
      LOG(ERROR) << "Read failure: " << data.status();
      ::close(fd);
      continue;
    }
    RequestMetadata request_metadata{
        .fd = fd,
        .token = data->substr(kNumTokenBytes),
        .handler =
            +[](int fd, std::filesystem::path /*log_file_name*/) {
              // The default handler is called when this thread is unblocked by
              // the destructor rather than an execution request.
              ::close(fd);
            },
    };
    std::filesystem::path log_file_name =
        log_dir_ / absl::StrCat(request_metadata.token, ".log");
    data->resize(kNumTokenBytes);
    {
      absl::MutexLock lock(&mu_);
      if (!code_token_to_request_metadatas_.contains(parent_code_token)) {
        // We do not immediately break here because we've already called
        // `accept` and must process the new connection first. Checking this
        // condition here allows us to only acquire the lock once per loop.
        parent_code_token_deleted = true;
      }
      if (const auto it = code_token_to_request_metadatas_.find(*data);
          it != code_token_to_request_metadatas_.end()) {
        it->second.push(&request_metadata);
      } else {
        LOG(INFO) << "Unrecognized code token.";
        ::close(fd);
        continue;
      }
    }
    request_metadata.ready.WaitForNotification();
    std::move(request_metadata.handler)(fd, std::move(log_file_name));
  }
  absl::MutexLock lock(&mu_);
  --acceptor_threads_in_flight_;
}
}  // namespace privacy_sandbox::server_common::byob
