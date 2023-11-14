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

#include "scp/cc/roma/benchmark/src/fake_ba_server.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"

namespace google::scp::roma::benchmark {
namespace {

using DispatchResponse = google::scp::roma::ResponseObject;
using LoadRequest = ::google::scp::roma::CodeObject;
using LoadResponse = ::google::scp::roma::ResponseObject;

constexpr absl::Duration kExecuteCodeTimeout = absl::Seconds(10);

}  // namespace

FakeBaServer::FakeBaServer(DispatchConfig config) {
  CHECK_OK(RomaInit(config));
}

FakeBaServer::~FakeBaServer() {
  CHECK_OK(RomaStop());
}

void FakeBaServer::LoadSync(int version, absl::string_view js) const {
  LoadRequest request;
  request.version_num = version;
  request.js = js;
  // Note: This is a BlockingCounter rather than a Notificaiton because that's
  // what B&A uses.
  absl::BlockingCounter is_loading(1);

  absl::Status try_load = google::scp::roma::LoadCodeObj(
      std::make_unique<LoadRequest>(request),
      [&is_loading](std::unique_ptr<absl::StatusOr<LoadResponse>> res) {
        CHECK_OK(*res);
        is_loading.DecrementCount();
      });
  CHECK_OK(try_load);
  is_loading.Wait();
}

void FakeBaServer::BatchExecute(std::vector<DispatchRequest>& batch) const {
  absl::Notification notification;
  auto batch_callback =
      [&notification](
          const std::vector<absl::StatusOr<DispatchResponse>>& result) {
        for (const auto& status_or : result) {
          CHECK_OK(status_or);
        }
        notification.Notify();
      };

  // This call schedules the code to be executed:
  CHECK_OK(google::scp::roma::BatchExecute(batch, std::move(batch_callback)));
  notification.WaitForNotificationWithTimeout(kExecuteCodeTimeout);
  CHECK(notification.HasBeenNotified()) << "Timed out waiting for UDF result.";
}

}  // namespace google::scp::roma::benchmark
