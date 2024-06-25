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

#include "src/roma/benchmark/fake_kv_server.h"

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

namespace google::scp::roma::benchmark {
namespace {

constexpr std::string_view kCodeObjectId = "id";  // Unused but required.
constexpr absl::Duration kCodeUpdateTimeout = absl::Seconds(1);
constexpr absl::Duration kExecuteCodeTimeout = absl::Seconds(10);
constexpr std::string_view kInvocationRequestId = "id";  // Unused but required.
constexpr std::string_view kVersionString = "v1";

}  // namespace

FakeKvServer::FakeKvServer(Config<> config) {
  roma_service_ =
      std::make_unique<google::scp::roma::sandbox::roma_service::RomaService<>>(
          std::move(config));
  CHECK_OK(roma_service_->Init());
}

FakeKvServer::~FakeKvServer() { CHECK_OK(roma_service_->Stop()); }

std::string FakeKvServer::ExecuteCode(std::vector<std::string> keys) {
  CHECK(handler_name_ != "")
      << "SetCodeObject() must be called before ExecuteCode()";

  absl::Status response_status;
  std::string result;
  absl::Notification notification;
  InvocationStrRequest<> invocation_request = {
      .id = std::string(kInvocationRequestId),
      .version_string = std::string(kVersionString),
      .handler_name = handler_name_,
      .input = std::move(keys),
  };

  CHECK_OK(roma_service_->Execute(
      std::make_unique<InvocationStrRequest<>>(invocation_request),
      [&notification, &response_status,
       &result](absl::StatusOr<ResponseObject> resp) {
        if (resp.ok()) {
          result = std::move(resp->resp);
        } else {
          response_status.Update(resp.status());
        }
        notification.Notify();
      }));
  CHECK(notification.WaitForNotificationWithTimeout(kExecuteCodeTimeout))
      << "ExecuteCode failed, timeout exceeded.";
  CHECK_OK(response_status);
  return result;
}

void FakeKvServer::SetCodeObject(CodeConfig code_config) {
  absl::Status response_status;
  absl::Notification notification;
  CodeObject code_object = {
      .id = std::string(kCodeObjectId),
      .version_string = std::string(kVersionString),
      .js = std::move(code_config.js),
      .wasm = std::move(code_config.wasm),
  };
  CHECK_OK(roma_service_->LoadCodeObj(
      std::make_unique<CodeObject>(std::move(code_object)),
      [&notification, &response_status](absl::StatusOr<ResponseObject> resp) {
        if (!resp.ok()) {
          response_status.Update(std::move(resp.status()));
        }
        notification.Notify();
      }));
  CHECK(notification.WaitForNotificationWithTimeout(kCodeUpdateTimeout))
      << "SetCodeObject failed, timeout exceeded.";
  CHECK_OK(response_status) << response_status;
  handler_name_ = std::move(code_config.udf_handler_name);
}

}  // namespace google::scp::roma::benchmark
