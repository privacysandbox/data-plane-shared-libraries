/*
 * Copyright 2024 Google LLC
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

using google::scp::roma::sandbox::roma_service::RomaService;
using ::testing::StrEq;

namespace google::scp::roma::test {
// This payload size is larger than the Buffer capacity.
constexpr double kOversizedPayloadSize = 1024 * 1024 * 1.2;

TEST(BufferSizeSapiTest,
     EnableBufferOnlyLoadingShouldFailGracefullyIfPayloadOversize) {
  Config config;
  config.number_of_workers = 2;
  // The buffer size is 1MB.
  config.sandbox_request_response_shared_buffer_size_mb = 1;
  config.enable_sandbox_sharing_request_response_with_buffer_only = true;

  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  absl::Notification load_finished;

  {
    std::string dummy_js_string(kOversizedPayloadSize, 'A');
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = absl::StrCat("\"", dummy_js_string, "\""),
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
    EXPECT_FALSE(response_status.ok());
    EXPECT_THAT(response_status.message(),
                StrEq("The size of request serialized data is "
                      "larger than the Buffer capacity."));
  }

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(BufferSizeSapiTest,
     EnableBufferOnlyExecutionShouldFailGracefullyIfRequestPayloadOversize) {
  Config config;
  config.number_of_workers = 2;
  // The buffer size is 1MB.
  config.sandbox_request_response_shared_buffer_size_mb = 1;
  config.enable_sandbox_sharing_request_response_with_buffer_only = true;

  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification success_execute_finished;
  absl::Notification failed_execute_finished;
  std::string retry_result;
  absl::Notification retry_success_execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
      function Handler(input) { return "Hello world! " + JSON.stringify(input);
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

  // execute success
  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("Foobar")"},
        });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response_status = resp.status();
                               result = std::move(resp->resp);
                               success_execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(success_execute_finished.WaitForNotificationWithTimeout(
        absl::Seconds(10)));
    ASSERT_TRUE(response_status.ok());
  }

  // Failure in execution as oversize input.
  {
    std::string dummy_string(kOversizedPayloadSize, 'A');

    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {absl::StrCat("\"", dummy_string, "\"")},
        });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response_status = resp.status();
                               failed_execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(failed_execute_finished.WaitForNotificationWithTimeout(
        absl::Seconds(10)));
    EXPECT_FALSE(response_status.ok());
    EXPECT_THAT(response_status.message(),
                StrEq("The size of request serialized data is larger than the "
                      "Buffer capacity."));
  }

  // execute success
  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("Foobar")"},
        });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response_status = resp.status();
                               retry_result = std::move(resp->resp);
                               retry_success_execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(retry_success_execute_finished.WaitForNotificationWithTimeout(
        absl::Seconds(10)));
    ASSERT_TRUE(response_status.ok());
  }

  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));
  EXPECT_THAT(retry_result, StrEq(R"("Hello world! \"Foobar\"")"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(BufferSizeSapiTest,
     EnableBufferOnlyExecutionShouldFailGracefullyIfResponsePayloadOversize) {
  Config config;
  config.number_of_workers = 10;
  // The buffer size is 1MB.
  config.sandbox_request_response_shared_buffer_size_mb = 1;
  config.enable_sandbox_sharing_request_response_with_buffer_only = true;

  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  absl::Notification load_finished;
  absl::Notification success_execute_finished;
  absl::Notification failed_execute_finished;
  absl::Notification retry_success_execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        // Will generate a response with input size.
        .js = R"JS_CODE(
        function Handler(input) {
          let dummy_string = 'x'.repeat(input);
          return "Hello world! " + JSON.stringify(dummy_string);
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

  // execute success
  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("1024")"},
        });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response_status = resp.status();
                               success_execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(success_execute_finished.WaitForNotificationWithTimeout(
        absl::Seconds(10)));
    EXPECT_TRUE(response_status.ok());
  }

  // execute failed as the response payload size is larger than buffer
  // capacity.
  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {absl::StrCat("\"", std::to_string(kOversizedPayloadSize),
                                   "\"")},
        });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response_status = resp.status();
                               failed_execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(failed_execute_finished.WaitForNotificationWithTimeout(
        absl::Seconds(10)));
    // Failure in execution
    EXPECT_EQ(response_status.code(), absl::StatusCode::kResourceExhausted);
  }

  // execute success
  {
    auto payload_size = 1024 * 800;
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {absl::StrCat("\"", std::to_string(payload_size), "\"")},
        });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response_status = resp.status();
                               retry_success_execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(retry_success_execute_finished.WaitForNotificationWithTimeout(
        absl::Seconds(10)));
    EXPECT_TRUE(response_status.ok());
  }

  ASSERT_TRUE(roma_service.Stop().ok());
}

}  // namespace google::scp::roma::test
