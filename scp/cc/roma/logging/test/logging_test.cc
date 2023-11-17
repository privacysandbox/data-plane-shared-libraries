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

#include <gtest/gtest.h>

#include "absl/base/log_severity.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/scoped_mock_log.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "roma/config/src/config.h"
#include "roma/interface/roma.h"

using google::scp::roma::FunctionBindingObjectV2;
using google::scp::roma::proto::FunctionBindingIoProto;
using ::testing::_;

namespace google::scp::roma::test {
TEST(LoggingTest, ShouldCallRegisteredLogFunctionBindings) {
  const std::vector<std::string> inputs{
      "str1",
      "str2",
      "str3",
  };

  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, _, inputs[0]));
  EXPECT_CALL(log, Log(absl::LogSeverity::kWarning, _, inputs[1]));
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, _, inputs[2]));
  log.StartCapturingLogs();

  auto status = RomaInit();
  EXPECT_TRUE(status.ok());

  absl::Notification load_finished;
  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_num = 1;
    code_obj->js = R"JS_CODE(
    function Handler(input1, input2, input3) {
      ROMA_LOG(input1);
      ROMA_WARN(input2);
      ROMA_ERROR(input3);
      return "Hello World";
    }
  )JS_CODE";

    status = google::scp::roma::LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          CHECK(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  std::string result;
  absl::Notification execute_finished;
  {
    auto execution_obj = std::make_unique<InvocationRequestStrInput>();
    execution_obj->id = "foo";
    execution_obj->version_num = 1;
    execution_obj->handler_name = "Handler";
    for (const auto& input : inputs) {
      execution_obj->input.push_back(absl::StrCat("\"", input, "\""));
    }

    // Execute UDF.
    status = google::scp::roma::Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          CHECK(resp->ok());
          auto& code_resp = **resp;
          result = code_resp.resp;
          execute_finished.Notify();
        });

    EXPECT_TRUE(status.ok());
  }

  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_EQ(result, R"("Hello World")");

  status = RomaStop();
  EXPECT_TRUE(status.ok());
  log.StopCapturingLogs();
}
}  // namespace google::scp::roma::test
