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

#include "src/roma/benchmark/fake_ba_server.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/roma_service/roma_service.h"

namespace google::scp::roma::benchmark {
namespace {

using DispatchResponse = google::scp::roma::ResponseObject;
using LoadRequest = ::google::scp::roma::CodeObject;
using LoadResponse = ::google::scp::roma::ResponseObject;

constexpr absl::Duration kExecuteCodeTimeout = absl::Seconds(10);

}  // namespace

FakeBaServer::FakeBaServer(DispatchConfig config) {
  roma_service_ =
      std::make_unique<google::scp::roma::sandbox::roma_service::RomaService<>>(
          std::move(config));
  CHECK_OK(roma_service_->Init());
}

FakeBaServer::~FakeBaServer() { CHECK_OK(roma_service_->Stop()); }

void FakeBaServer::LoadSync(std::string_view version,
                            std::string_view js) const {
  LoadRequest request;
  request.version_string = version;
  request.js = js;
  // Note: This is a BlockingCounter rather than a Notification because that's
  // what B&A uses.
  absl::BlockingCounter is_loading(1);

  CHECK_OK(roma_service_->LoadCodeObj(
      std::make_unique<LoadRequest>(request),
      [&is_loading](absl::StatusOr<LoadResponse> res) {
        CHECK_OK(res);
        is_loading.DecrementCount();
      }));
  is_loading.Wait();
}

void FakeBaServer::BatchExecute(std::vector<DispatchRequest>& batch) const {
  using BatchCallback =
      absl::AnyInvocable<void(std::vector<absl::StatusOr<ResponseObject>>)>;

  absl::Notification notification;
  BatchCallback batch_callback =
      [&notification](
          const std::vector<absl::StatusOr<DispatchResponse>>& result) {
        for (const auto& status_or : result) {
          CHECK_OK(status_or);
        }
        notification.Notify();
      };

  const size_t batch_size = batch.size();
  auto batch_response =
      std::make_shared<std::vector<absl::StatusOr<DispatchResponse>>>(
          batch_size, absl::StatusOr<DispatchResponse>());
  auto finished_counter = std::make_shared<std::atomic<size_t>>(0);
  auto batch_callback_ptr =
      std::make_shared<BatchCallback>(std::move(batch_callback));

  for (size_t index = 0; index < batch_size; ++index) {
    auto single_callback =
        [batch_response, finished_counter, batch_callback_ptr,
         index](absl::StatusOr<DispatchResponse> obj_response) {
          (*batch_response)[index] = std::move(obj_response);
          auto finished_value = finished_counter->fetch_add(1);
          if (finished_value + 1 == batch_response->size()) {
            (*batch_callback_ptr)(std::move(*batch_response));
          }
        };

    absl::Status result;
    while (!(result =
                 roma_service_
                     ->Execute(std::make_unique<DispatchRequest>(batch[index]),
                               single_callback)
                     .status())
                .ok()) {
      // If the first request from the batch got a failure, return failure
      // without waiting.
      CHECK_NE(index, 0);
    }
  }

  notification.WaitForNotificationWithTimeout(kExecuteCodeTimeout);
  CHECK(notification.HasBeenNotified()) << "Timed out waiting for UDF result.";
}

}  // namespace google::scp::roma::benchmark
