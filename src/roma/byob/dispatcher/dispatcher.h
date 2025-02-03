/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_ROMA_BYOB_DISPATCHER_DISPATCHER_H_
#define SRC_ROMA_BYOB_DISPATCHER_DISPATCHER_H_

#include <fcntl.h>
#include <sys/mman.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/byob/dispatcher/dispatcher.grpc.pb.h"
#include "src/roma/byob/utility/file_reader.h"
#include "src/util/execution_token.h"

namespace privacy_sandbox::server_common::byob {

class Dispatcher {
 public:
  ~Dispatcher();

  absl::Status Init(std::filesystem::path control_socket_name,
                    std::filesystem::path udf_socket_name,
                    std::filesystem::path log_dir);

  absl::StatusOr<std::string> LoadBinary(std::filesystem::path binary_path,
                                         int num_workers,
                                         bool enable_log_egress = false)
      ABSL_LOCKS_EXCLUDED(mu_);

  absl::StatusOr<std::string> LoadBinaryForLogging(
      std::string source_bin_code_token, int num_workers)
      ABSL_LOCKS_EXCLUDED(mu_);

  void Delete(std::string_view code_token) ABSL_LOCKS_EXCLUDED(mu_);

  void Cancel(google::scp::roma::ExecutionToken execution_token);

  template <typename Response, typename Request>
  absl::StatusOr<google::scp::roma::ExecutionToken> ProcessRequest(
      std::string_view code_token, const Request& request,
      absl::AnyInvocable<void(absl::StatusOr<Response>,
                              absl::StatusOr<std::string_view> logs) &&>
          callback) ABSL_LOCKS_EXCLUDED(mu_) {
    RequestMetadata* request_metadata;
    {
      absl::MutexLock l(&mu_);
      const auto it = code_token_to_request_metadatas_.find(code_token);
      if (it == code_token_to_request_metadatas_.end()) {
        return absl::InvalidArgumentError("Unrecognized code token.");
      }
      if (it->second.empty()) {
        return absl::UnavailableError("No workers available.");
      }
      request_metadata = it->second.front();
      it->second.pop();
    }
    request_metadata->handler =
        [callback = std::move(callback)](
            const int fd, std::filesystem::path log_file_name) mutable {
          Response response;
          if (google::protobuf::io::FileInputStream input(fd);
              !google::protobuf::util::ParseDelimitedFromZeroCopyStream(
                  &response, &input, nullptr)) {
            std::move(callback)(
                absl::UnavailableError("No UDF response received."),
                FileReader::GetContent(FileReader::Create(log_file_name)));
          } else {
            std::move(callback)(
                std::move(response),
                FileReader::GetContent(FileReader::Create(log_file_name)));
          }
          ::close(fd);
        };
    google::scp::roma::ExecutionToken execution_token{
        std::move(request_metadata->token)};
    request_metadata->ready.Notify();
    google::protobuf::util::SerializeDelimitedToFileDescriptor(
        request, request_metadata->fd);
    return execution_token;
  }

 private:
  struct RequestMetadata {
    int fd;
    std::string token;
    absl::AnyInvocable<void(int, std::filesystem::path) &&> handler;
    absl::Notification ready;
  };

  // Accepts connections from newly created UDF instances, reads code tokens,
  // and pushes file descriptors to the queue.
  void AcceptorImpl(std::string parent_code_token) ABSL_LOCKS_EXCLUDED(mu_);

  int listen_fd_;
  std::filesystem::path log_dir_;
  std::unique_ptr<WorkerRunnerService::Stub> stub_;
  absl::Mutex mu_;
  int acceptor_threads_in_flight_ ABSL_GUARDED_BY(mu_) = 0;
  absl::flat_hash_map<std::string, std::queue<RequestMetadata*>>
      code_token_to_request_metadatas_ ABSL_GUARDED_BY(mu_);
};
}  // namespace privacy_sandbox::server_common::byob

#endif  // SRC_ROMA_BYOB_DISPATCHER_DISPATCHER_H_
