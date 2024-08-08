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

#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/interface/function_binding_io.pb.h"

namespace privacy_sandbox::server_common::gvisor {
class Dispatcher {
 public:
  ~Dispatcher();

  // Takes ownership of `listen_fd`. Blocks until a connection is established.
  absl::Status Init(int listen_fd);

  std::string LoadBinary(std::string_view binary_path, int n_workers);

  template <typename Table, typename Metadata>
  void ExecuteBinary(
      std::string_view code_token, std::string serialized_request,
      Metadata metadata, const Table& table,
      absl::AnyInvocable<void(absl::StatusOr<std::string>) &&> callback)
      ABSL_LOCKS_EXCLUDED(mu_) {
    Fds fds;
    {
      auto fn = [&] {
        mu_.AssertReaderHeld();
        return !code_token_to_fds_[code_token].empty();
      };
      absl::MutexLock l(&mu_);
      mu_.Await(absl::Condition(&fn));
      const auto it = code_token_to_fds_.find(code_token);
      fds = it->second.front();
      it->second.pop();
      ++handler_threads_in_flight_;
    }
    std::thread(&Dispatcher::ExecutorImpl, fds.io_fd,
                std::move(serialized_request), std::move(callback))
        .detach();
    std::thread(&Dispatcher::HandlerImpl, this, fds.callback_fd,
                [&table, metadata = std::move(metadata)](
                    std::string_view function, auto& io_proto) {
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
  }

 private:
  // Accepts connections from newly created UDF instances, reads code tokens,
  // and pushes file descriptors to the queue.
  void AcceptorImpl() ABSL_LOCKS_EXCLUDED(mu_);
  static void ExecutorImpl(
      int io_fd, std::string serialized_request,
      absl::AnyInvocable<void(absl::StatusOr<std::string>) &&> callback);
  void HandlerImpl(
      int callback_fd,
      absl::FunctionRef<void(std::string_view,
                             google::scp::roma::proto::FunctionBindingIoProto&)>
          handler) ABSL_LOCKS_EXCLUDED(mu_);

  int listen_fd_;

  // Connection socket to worker main thread. Used to send load requests.
  int connection_fd_;
  std::optional<std::thread> acceptor_;
  absl::Mutex mu_;
  int handler_threads_in_flight_ ABSL_GUARDED_BY(mu_) = 0;

  struct Fds {
    int io_fd;
    int callback_fd;
  };
  absl::flat_hash_map<std::string, std::queue<Fds>> code_token_to_fds_
      ABSL_GUARDED_BY(mu_);
};
}  // namespace privacy_sandbox::server_common::gvisor

#endif  // SRC_ROMA_GVISOR_DISPATCHER_DISPATCHER_H_
