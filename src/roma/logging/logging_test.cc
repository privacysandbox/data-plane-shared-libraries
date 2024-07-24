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
#include "absl/log/log.h"
#include "absl/log/scoped_mock_log.h"
#include "absl/status/status.h"
#include "absl/synchronization/notification.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

using google::scp::roma::sandbox::roma_service::RomaService;
using ::testing::_;
using ::testing::AllOf;
using ::testing::HasSubstr;

namespace google::scp::roma::test {
namespace {
constexpr auto kTimeout = absl::Seconds(10);
}

void LoggingFunction(absl::LogSeverity severity,
                     absl::flat_hash_map<std::string, std::string> metadata,
                     std::string_view msg) {
  LOG(LEVEL(severity)) << msg;
}

TEST(LoggingTest, ConsoleLoggingNoOpWhenMinLogLevelSet) {
  Config config;
  config.number_of_workers = 2;
  config.SetLoggingFunction(LoggingFunction);
  RomaService<> roma_service(std::move(config));
  EXPECT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, testing::_, "Hello World"))
      .Times(0);
  EXPECT_CALL(log, Log(absl::LogSeverity::kWarning, testing::_, "Hello World"));
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_, "Hello World"));
  log.StartCapturingLogs();
  {
    const std::string js = R"(
      function Handler() {
        console.log("Hello World");
        console.warn("Hello World");
        console.error("Hello World");
      }
    )";

    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = js,
    });

    absl::Status response_status;
    EXPECT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response_status = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(kTimeout));
    EXPECT_TRUE(response_status.ok());
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .min_log_level = absl::LogSeverity::kWarning,
        });

    absl::Status response_status;
    EXPECT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response_status = resp.status();
                               if (resp.ok()) {
                                 result = std::move(resp->resp);
                               }
                               execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(execute_finished.WaitForNotificationWithTimeout(kTimeout));
    EXPECT_TRUE(response_status.ok());
  }

  EXPECT_TRUE(roma_service.Stop().ok());
  log.StopCapturingLogs();
}

TEST(LoggingTest, StackTracesLoggedWhenLoggingFunctionSet) {
  Config config;
  config.number_of_workers = 2;
  config.SetLoggingFunction(LoggingFunction);
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  absl::Notification load_finished;
  absl::Notification execute_failed;

  absl::ScopedMockLog log;
  constexpr std::string_view input = "Hello World";
  EXPECT_CALL(
      log, Log(absl::LogSeverity::kError, _,
               AllOf(HasSubstr(absl::StrCat("Uncaught Error: ", input)),
                     HasSubstr("at ErrorFunction"), HasSubstr("at Handler"))))
      .Times(2);
  log.StartCapturingLogs();

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
      function ErrorFunction(input) {
        throw new Error(input);
      }

      function Handler(input) {
        ErrorFunction(input);
      }
    )JS_CODE",
    });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response_status = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(response_status.ok());
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {absl::StrCat("\"", input, "\"")},
        });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response_status = resp.status();
                               execute_failed.Notify();
                             })
                    .ok());
    ASSERT_TRUE(
        execute_failed.WaitForNotificationWithTimeout(absl::Seconds(10)));
    EXPECT_EQ(response_status.code(), absl::StatusCode::kInternal);
  }

  ASSERT_TRUE(roma_service.Stop().ok());
  log.StopCapturingLogs();
}
}  // namespace google::scp::roma::test
