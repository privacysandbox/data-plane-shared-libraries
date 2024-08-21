/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ROMA_SANDBOX_DISPATCHER_DISPATCHER_H_
#define ROMA_SANDBOX_DISPATCHER_DISPATCHER_H_

#include <queue>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "src/roma/interface/roma.h"
#include "src/roma/sandbox/worker_api/sapi/worker_params.pb.h"
#include "src/roma/sandbox/worker_api/sapi/worker_sandbox_api.h"
#include "src/util/execution_token.h"
#include "src/util/status_macro/status_macros.h"

#include "request_converter.h"
#include "request_validator.h"

namespace google::scp::roma::sandbox::dispatcher {
class Dispatcher final {
 public:
  // Starts a thread for each worker.
  Dispatcher(absl::Span<worker_api::WorkerSandboxApi> workers,
             int max_pending_requests)
      : max_pending_requests_(max_pending_requests),
        workers_(workers),
        per_worker_requests_(workers_.size()) {
    CHECK(!workers_.empty());
    consumers_.reserve(workers_.size());
    for (int i = 0; i < workers_.size(); ++i) {
      consumers_.emplace_back(&Dispatcher::ConsumerImpl, this, i);
    }
  }

  // Clears queue and kills threads.
  ~Dispatcher();

  // Queues a CodeObject to be loaded on all workers.
  absl::Status Load(
      CodeObject code_object,
      absl::AnyInvocable<void(absl::StatusOr<ResponseObject>) &&> callback)
      ABSL_LOCKS_EXCLUDED(mu_);

  // Queues a CodeObject or InvocationRequest to be invoked by a worker.
  template <typename RequestT>
  absl::Status Invoke(
      RequestT request,
      absl::AnyInvocable<void(absl::StatusOr<ResponseObject>) &&> callback)
      ABSL_LOCKS_EXCLUDED(mu_) {
    PS_RETURN_IF_ERROR(AssertRequestIsValid(request));
    ::worker_api::WorkerParamsProto param = RequestToProto(std::move(request));
    absl::MutexLock lock(&mu_);
    if (requests_.size() == max_pending_requests_) {
      return absl::ResourceExhaustedError(
          "Dispatch is disallowed since the number of unfinished requests is "
          "at capacity.");
    }
    requests_.push(Request{
        .param = std::move(param),
        .callback = std::move(callback),
    });
    return absl::OkStatus();
  }

  void Cancel(const ExecutionToken& token);

 private:
  struct Request {
    ::worker_api::WorkerParamsProto param;
    absl::AnyInvocable<void(absl::StatusOr<ResponseObject>) &&> callback;
  };

  // While-loop pulls requests off queue and processes them with a
  // fixed worker.
  void ConsumerImpl(int i);

  int max_pending_requests_;
  absl::Span<worker_api::WorkerSandboxApi> workers_;
  std::vector<std::thread> consumers_;
  absl::Mutex mu_;

  // Consumers break out of while-loop when true.
  bool kill_consumers_ ABSL_GUARDED_BY(mu_) = false;

  // `params_` contains code to reload onto failed workers.
  std::vector<::worker_api::WorkerParamsProto> params_ ABSL_GUARDED_BY(mu_);

  // `requests_` holds invocation requests.
  // TODO(b/325486969): Replace `std::queue` with bounded queue implementation.
  std::queue<Request> requests_ ABSL_GUARDED_BY(mu_);

  // `per_worker_requests_[i]` holds code objects to load.
  std::vector<std::queue<Request>> per_worker_requests_ ABSL_GUARDED_BY(mu_);
};
}  // namespace google::scp::roma::sandbox::dispatcher

#endif  // ROMA_SANDBOX_DISPATCHER_DISPATCHER_H_
