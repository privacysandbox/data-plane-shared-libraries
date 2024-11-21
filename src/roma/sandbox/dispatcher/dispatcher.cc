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

#include "dispatcher.h"

#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "src/roma/interface/roma.h"
#include "src/roma/logging/logging.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/worker_api/sapi/utils.h"
#include "src/roma/sandbox/worker_api/sapi/worker_params.pb.h"
#include "src/util/duration.h"
#include "src/util/execution_token.h"
#include "src/util/protoutil.h"
#include "src/util/status_macro/status_macros.h"

namespace google::scp::roma::sandbox::dispatcher {
using google::scp::roma::sandbox::constants::kRequestId;
using google::scp::roma::sandbox::constants::kRequestUuid;
using google::scp::roma::sandbox::worker_api::RetryStatus;

Dispatcher::~Dispatcher() {
  // Wait for per-worker queues to empty to ensure cleanup runs.
  auto fn = [&] {
    mu_.AssertReaderHeld();
    for (const auto& queue : per_worker_requests_) {
      if (!queue.empty()) {
        return false;
      }
    }
    return true;
  };
  {
    absl::MutexLock lock(&mu_);
    mu_.Await(absl::Condition(&fn));
    requests_ = {};
    kill_consumers_ = true;
  }
  for (std::thread& consumer : consumers_) {
    consumer.join();
  }
}

absl::Status Dispatcher::Load(
    CodeObject code_object,
    absl::AnyInvocable<void(absl::StatusOr<ResponseObject>) &&> callback) {
  PS_RETURN_IF_ERROR(AssertRequestIsValid(code_object));
  ::worker_api::WorkerParamsProto param =
      RequestToProto(std::move(code_object));
  const int n_workers = workers_.size();
  if (n_workers == 1) {
    absl::MutexLock lock(&mu_);
    per_worker_requests_.front().push(Request{
        .param = param,
        .callback =
            [callback = std::move(callback)](auto response) mutable {
              std::move(callback)(std::move(response));
            },
    });
    params_.push_back(std::move(param));
  } else {
    auto* const mu = new absl::Mutex;
    auto* const counter = new int(0);
    auto* const shared_callback = new decltype(callback)(std::move(callback));
    absl::MutexLock lock(&mu_);
    for (auto& queue : per_worker_requests_) {
      queue.push(Request{
          .param = param,
          .callback =
              [=](auto response) {
                const int count = [=] {
                  absl::MutexLock lock(mu);
                  return ++(*counter);
                }();
                if (count == 1) {
                  // Assume success or failure of later loads will match first.
                  std::move (*shared_callback)(std::move(response));
                  delete shared_callback;
                } else if (count == n_workers) {
                  // This block is guaranteed to run and to only run once
                  // because every one of the `n_workers` `Request::callback`s
                  // created by this load request is guaranteed to run.
                  delete mu;
                  delete counter;
                }
              },
      });
    }
    params_.push_back(std::move(param));
  }
  return absl::OkStatus();
}

void Dispatcher::ConsumerImpl(int i) {
  while (true) {
    Request request;
    {
      auto fn = [&] {
        mu_.AssertReaderHeld();
        return kill_consumers_ || !per_worker_requests_[i].empty() ||
               !requests_.empty();
      };
      absl::MutexLock lock(&mu_);
      mu_.Await(absl::Condition(&fn));
      if (kill_consumers_) {
        break;
      }

      // Prioritize loading over executing.
      if (!per_worker_requests_[i].empty()) {
        request = std::move(per_worker_requests_[i].front());
        per_worker_requests_[i].pop();
      } else {
        request = std::move(requests_.front());
        requests_.pop();
      }
    }
    privacy_sandbox::server_common::Stopwatch stopwatch;
    if (auto [error, retry_status] = workers_[i].RunCode(request.param);
        !error.ok()) {
      LOG(ERROR) << "The worker " << i << " execute the request failed due to "
                 << error;
      if (retry_status == RetryStatus::kRetry) {
        // This means that the worker crashed and the request could be retried,
        // however, we need to reload the worker with the cached code.
        std::vector<::worker_api::WorkerParamsProto> params;
        {
          absl::MutexLock lock(&mu_);
          params = params_;
        }
        for (::worker_api::WorkerParamsProto& param : params) {
          if (auto [status, _] = workers_[i].RunCode(param); !status.ok()) {
            LOG(ERROR) << "Reloading the worker cache failed with " << status;
            break;
          }
        }
        ROMA_VLOG(1)
            << "Successfully reload all cached code objects to the worker"
            << index;
      }
      std::move(request).callback(std::move(error));
    } else {
      absl::Duration run_code_duration = stopwatch.GetElapsedTime();
      ResponseObject response;
      response.metrics.reserve(1 + request.param.metrics_size());
      response.metrics[roma::sandbox::constants::
                           kExecutionMetricSandboxedJsEngineCallDuration] =
          std::move(run_code_duration);
      absl::Status status = [&] {
        for (auto& [key, proto_duration] : *request.param.mutable_metrics()) {
          PS_ASSIGN_OR_RETURN(
              response.metrics[std::move(key)],
              privacy_sandbox::server_common::DecodeGoogleApiProto(
                  proto_duration));
        }
        return absl::OkStatus();
      }();
      if (!status.ok()) {
        std::move(request).callback(std::move(status));
      } else {
        response.id =
            std::move((*request.param.mutable_metadata())[kRequestId]);
        response.resp = std::move(*request.param.mutable_response());
        response.profiler_output =
            std::move(*request.param.mutable_profiler_output());
        std::move(request).callback(std::move(response));
      }
    }
  }
}

void Dispatcher::Cancel(const ExecutionToken& token) {
  absl::MutexLock lock(&mu_);

  size_t num_queued_requests = requests_.size();
  for (int i = 0; i < num_queued_requests; i++) {
    Request item = std::move(requests_.front());
    requests_.pop();

    if (item.param.metadata().at(kRequestUuid) != token.value) {
      requests_.push(std::move(item));
    } else {
      std::move(item).callback(
          absl::CancelledError("Request has been cancelled."));
    }
  }
}
}  // namespace google::scp::roma::sandbox::dispatcher
