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

#ifndef SRC_ROMA_GVISOR_DISPATCHER_DISPATCHER_H_
#define SRC_ROMA_GVISOR_DISPATCHER_DISPATCHER_H_

#include <fcntl.h>
#include <sys/mman.h>

#include <filesystem>
#include <fstream>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/functional/function_ref.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "google/protobuf/any.pb.h"
#include "src/roma/byob/utility/file_reader.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/interface/function_binding_io.pb.h"
#include "src/util/execution_token.h"

namespace privacy_sandbox::server_common::byob {
inline constexpr size_t kNumTokenBytes = 36;

class Dispatcher {
 public:
  ~Dispatcher();

  // Takes ownership of `listen_fd`. Blocks until a connection is established.
  absl::Status Init(int listen_fd, std::filesystem::path log_dir);

  absl::StatusOr<std::string> LoadBinary(std::filesystem::path binary_path,
                                         int num_workers,
                                         bool enable_log_egress = false)
      ABSL_LOCKS_EXCLUDED(mu_);

  absl::StatusOr<std::string> LoadBinaryForLogging(
      std::string source_bin_code_token, int num_workers)
      ABSL_LOCKS_EXCLUDED(mu_);

  void Delete(std::string_view code_token) ABSL_LOCKS_EXCLUDED(mu_);

  void Cancel(google::scp::roma::ExecutionToken execution_token);

  template <typename Response, typename Table, typename Metadata,
            typename Request>
  absl::StatusOr<google::scp::roma::ExecutionToken> ProcessRequest(
      std::string_view code_token, const Request& request, Metadata metadata,
      const Table& table,
      absl::AnyInvocable<void(absl::StatusOr<Response>,
                              absl::StatusOr<std::string_view> logs) &&>
          callback) ABSL_LOCKS_EXCLUDED(mu_) {
    google::protobuf::Any request_any;
    request_any.PackFrom(request);
    FdAndToken fd_and_token;
    {
      auto fn = [&] {
        mu_.AssertReaderHeld();
        const auto it = code_token_to_fds_and_tokens_.find(code_token);
        return it == code_token_to_fds_and_tokens_.end() || !it->second.empty();
      };
      absl::MutexLock l(&mu_);
      mu_.Await(absl::Condition(&fn));
      const auto it = code_token_to_fds_and_tokens_.find(code_token);
      if (it == code_token_to_fds_and_tokens_.end()) {
        return absl::InvalidArgumentError("Unrecognized code token.");
      }
      fd_and_token = std::move(it->second.front());
      it->second.pop();
      ++executor_threads_in_flight_;
    }
    std::thread(
        &Dispatcher::ExecutorImpl, this, fd_and_token.fd,
        std::move(request_any),
        [callback = std::move(callback),
         log_file_name = log_dir_ / absl::StrCat(fd_and_token.token, ".log")](
            absl::StatusOr<google::protobuf::Any> response_any) mutable {
          absl::StatusOr<FileReader> log_reader =
              FileReader::Create(log_file_name);
          if (!response_any.ok()) {
            std::move(callback)(std::move(response_any).status(),
                                FileReader::GetContent(log_reader));
          } else if (Response response; response_any->UnpackTo(&response)) {
            std::move(callback)(std::move(response),
                                FileReader::GetContent(log_reader));
          } else {
            std::move(callback)(absl::UnknownError("Failed to unpack output."),
                                FileReader::GetContent(log_reader));
          }
        },
        [&table, metadata = std::move(metadata)](std::string_view function,
                                                 auto& io_proto) {
          if (const auto it = table.find(function); it != table.end()) {
            google::scp::roma::FunctionBindingPayload<Metadata> wrapper{
                .io_proto = io_proto,
                .metadata = metadata,
            };
            (it->second)(wrapper);
          } else {
            io_proto.mutable_errors()->Add(
                "ROMA: Could not find C++ function by name.");
          }
        })
        .detach();
    return google::scp::roma::ExecutionToken{std::move(fd_and_token).token};
  }

 private:
  struct FdAndToken {
    int fd;
    std::string token;
  };

  // Accepts connections from newly created UDF instances, reads code tokens,
  // and pushes file descriptors to the queue.
  void AcceptorImpl() ABSL_LOCKS_EXCLUDED(mu_);
  void ExecutorImpl(
      int fd, google::protobuf::Any request,
      absl::AnyInvocable<void(absl::StatusOr<google::protobuf::Any>) &&>
          callback,
      absl::FunctionRef<void(std::string_view,
                             google::scp::roma::proto::FunctionBindingIoProto&)>
          handler) ABSL_LOCKS_EXCLUDED(mu_);

  int listen_fd_;

  // Connection socket to worker main thread. Used to send load requests.
  int connection_fd_;
  std::filesystem::path log_dir_;
  std::optional<std::thread> acceptor_;
  absl::Mutex mu_;
  int executor_threads_in_flight_ ABSL_GUARDED_BY(mu_) = 0;
  absl::flat_hash_map<std::string, std::queue<FdAndToken>>
      code_token_to_fds_and_tokens_ ABSL_GUARDED_BY(mu_);
};
}  // namespace privacy_sandbox::server_common::byob

#endif  // SRC_ROMA_GVISOR_DISPATCHER_DISPATCHER_H_
