//  Copyright 2025 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include "src/clients/http_fetcher_async/curl_request_worker.h"

#include <string>
#include <utility>

#include "absl/synchronization/mutex.h"

namespace privacy_sandbox::server_common::clients {

CurlRequestWorker::CurlRequestWorker(
    Executor* executor, CurlRequestQueue& request_queue,
    const int64_t curlmopt_maxconnects,
    const int64_t curlmopt_max_total_connections,
    const int64_t curlmopt_max_host_connections)
    : executor_(executor),
      request_queue_(request_queue),
      multi_curl_request_manager_(curlmopt_maxconnects,
                                  curlmopt_max_total_connections,
                                  curlmopt_max_host_connections, *executor) {
  // Start processing thread.
  executor_->Run([this]() { ProcessRequests(); });
}

CurlRequestWorker::~CurlRequestWorker() {
  {
    absl::MutexLock lock(&request_queue_.Mu());
    shutdown_requested_ = true;
  }
  shutdown_complete_.WaitForNotification();
}

void CurlRequestWorker::ProcessRequests() {
  CurlRequestWaiterArg waiter_arg = {
      .queue = request_queue_,
      .shutdown_requested = shutdown_requested_,
  };
  while (true) {
    std::unique_ptr<CurlRequestData> request;
    {
      absl::MutexLock lock(&request_queue_.Mu(),
                           absl::Condition(
                               +[](CurlRequestWaiterArg* waiter_arg) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
                                 return !waiter_arg->queue.Empty() ||
                                        waiter_arg->shutdown_requested;
#pragma clang diagnostic pop
                               },
                               &waiter_arg));
      if (shutdown_requested_) {
        break;
      }
      request = request_queue_.Dequeue();
    }
    multi_curl_request_manager_.StartProcessing(std::move(request));
  }
  shutdown_complete_.Notify();
}

}  // namespace privacy_sandbox::server_common::clients
