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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

using google::scp::roma::sandbox::roma_service::RomaService;
using ::testing::StrEq;

namespace google::scp::roma::test {

TEST(BufferSizeTest, LoadingShouldSucceedIfPayloadLargerThanBufferSize) {
  Config config;
  config.number_of_workers = 2;
  // The buffer size is 1MB.
  config.sandbox_request_response_shared_buffer_size_mb = 1;
  config.enable_sandbox_sharing_request_response_with_buffer_only = false;

  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification success_execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    // The js payload size is larger than the Buffer capacity.
    auto payload_size = 1024 * 1024 * 1.2;
    std::string dummy_js_string(payload_size, 'a');
    code_obj->js = "function Handler(input) { let x = \"" + dummy_js_string +
                   "\"; return \"Hello world! \"}";
    EXPECT_GE(code_obj->js.length(), payload_size);

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    ASSERT_TRUE(status.ok());
  }

  // execute success
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back("\"Foobar\"");

    status = roma_service.Execute(std::move(execution_obj),
                                  [&](absl::StatusOr<ResponseObject> resp) {
                                    ASSERT_TRUE(resp.ok());
                                    result = std::move(resp->resp);
                                    success_execute_finished.Notify();
                                  });
    EXPECT_TRUE(status.ok());
  }

  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(success_execute_finished.WaitForNotificationWithTimeout(
      absl::Seconds(10)));
  EXPECT_THAT(result, StrEq(R"("Hello world! ")"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(BufferSizeTest, ExecutionShouldSucceedIfRequestPayloadOversize) {
  Config config;
  config.number_of_workers = 2;
  // The buffer size is 1MB.
  config.sandbox_request_response_shared_buffer_size_mb = 1;
  config.enable_sandbox_sharing_request_response_with_buffer_only = false;

  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification oversize_execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE";

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    // The input payload size is larger than the Buffer capacity.
    auto payload_size = 1024 * 1024 * 1.2;
    std::string dummy_string(payload_size, 'A');
    execution_obj->input.push_back("\"" + dummy_string + "\"");

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          ASSERT_TRUE(resp.ok());
          EXPECT_GE(resp->resp.length(), payload_size);
          oversize_execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(oversize_execute_finished.WaitForNotificationWithTimeout(
      absl::Seconds(10)));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(BufferSizeTest, ExecutionShouldSucceedIfResponsePayloadOversize) {
  Config config;
  config.number_of_workers = 2;
  // The buffer size is 1MB.
  config.sandbox_request_response_shared_buffer_size_mb = 1;
  config.enable_sandbox_sharing_request_response_with_buffer_only = false;

  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  absl::Notification load_finished;
  absl::Notification oversize_execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    // Will generate a response with input size.
    code_obj->js = R"JS_CODE(
    function Handler(input) {
      let dummy_string = 'x'.repeat(input);
      return "Hello world! " + JSON.stringify(dummy_string);
    }
  )JS_CODE";

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
  }

  // execute success when the response payload size is larger than buffer
  // capacity.
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    // The response payload size is larger than the Buffer capacity.
    auto payload_size = 1024 * 1024 * 1.2;
    execution_obj->input.push_back("\"" + std::to_string(payload_size) + "\"");

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          ASSERT_TRUE(resp.ok());
          EXPECT_GE(resp->resp.length(), payload_size);
          oversize_execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(oversize_execute_finished.WaitForNotificationWithTimeout(
      absl::Seconds(10)));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(BufferSizeTest,
     EnableBufferOnlyLoadingShouldFailGracefullyIfPayloadOversize) {
  Config config;
  config.number_of_workers = 2;
  // The buffer size is 1MB.
  config.sandbox_request_response_shared_buffer_size_mb = 1;
  config.enable_sandbox_sharing_request_response_with_buffer_only = true;

  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    // The js payload size is larger than the Buffer capacity.
    auto payload_size = 1024 * 1024 * 1.2;
    std::string dummy_js_string(payload_size, 'A');
    code_obj->js = "\"" + dummy_js_string + "\"";

    status = roma_service.LoadCodeObj(
        std::move(code_obj), [&](absl::StatusOr<ResponseObject> resp) {
          EXPECT_FALSE(resp.ok());
          EXPECT_THAT(resp.status().message(),
                      StrEq("The size of request serialized data is "
                            "larger than the Buffer capacity."));
          load_finished.Notify();
        });
    ASSERT_TRUE(status.ok());
  }

  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(BufferSizeTest,
     EnableBufferOnlyExecutionShouldFailGracefullyIfRequestPayloadOversize) {
  Config config;
  config.number_of_workers = 2;
  // The buffer size is 1MB.
  config.sandbox_request_response_shared_buffer_size_mb = 1;
  config.enable_sandbox_sharing_request_response_with_buffer_only = true;

  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification success_execute_finished;
  absl::Notification failed_execute_finished;
  std::string retry_result;
  absl::Notification retry_success_execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE";

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
  }

  // execute success
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back("\"Foobar\"");

    status = roma_service.Execute(std::move(execution_obj),
                                  [&](absl::StatusOr<ResponseObject> resp) {
                                    ASSERT_TRUE(resp.ok());
                                    result = std::move(resp->resp);
                                    success_execute_finished.Notify();
                                  });
    EXPECT_TRUE(status.ok());
  }

  // Failure in execution as oversize input.
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    // The input payload size is larger than the Buffer capacity.
    auto payload_size = 1024 * 1024 * 1.2;
    std::string dummy_string(payload_size, 'A');
    execution_obj->input.push_back("\"" + dummy_string + "\"");

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          EXPECT_FALSE(resp.ok());
          EXPECT_THAT(resp.status().message(),
                      StrEq("The size of request serialized data is "
                            "larger than the Buffer capacity."));
          failed_execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  // execute success
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back("\"Foobar\"");

    status = roma_service.Execute(std::move(execution_obj),
                                  [&](absl::StatusOr<ResponseObject> resp) {
                                    ASSERT_TRUE(resp.ok());
                                    retry_result = std::move(resp->resp);
                                    retry_success_execute_finished.Notify();
                                  });
    EXPECT_TRUE(status.ok());
  }

  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(success_execute_finished.WaitForNotificationWithTimeout(
      absl::Seconds(10)));
  ASSERT_TRUE(failed_execute_finished.WaitForNotificationWithTimeout(
      absl::Seconds(10)));
  retry_success_execute_finished.WaitForNotificationWithTimeout(
      absl::Seconds(10));
  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));
  EXPECT_THAT(retry_result, StrEq(R"("Hello world! \"Foobar\"")"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(BufferSizeTest,
     EnableBufferOnlyExecutionShouldFailGracefullyIfResponsePayloadOversize) {
  Config config;
  config.number_of_workers = 10;
  // The buffer size is 1MB.
  config.sandbox_request_response_shared_buffer_size_mb = 1;
  config.enable_sandbox_sharing_request_response_with_buffer_only = true;

  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  absl::Notification load_finished;
  absl::Notification success_execute_finished;
  absl::Notification failed_execute_finished;
  absl::Notification retry_success_execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    // Will generate a response with input size.
    code_obj->js = R"JS_CODE(
    function Handler(input) {
      let dummy_string = 'x'.repeat(input);
      return "Hello world! " + JSON.stringify(dummy_string);
    }
  )JS_CODE";

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
  }

  // execute success
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back("\"1024\"");

    status = roma_service.Execute(std::move(execution_obj),
                                  [&](absl::StatusOr<ResponseObject> resp) {
                                    EXPECT_TRUE(resp.ok());
                                    success_execute_finished.Notify();
                                  });
    EXPECT_TRUE(status.ok());
  }

  // execute failed as the response payload size is larger than buffer
  // capacity.
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    // The response payload size is larger than the Buffer capacity.
    auto payload_size = 1024 * 1024 * 1.2;
    execution_obj->input.push_back("\"" + std::to_string(payload_size) + "\"");

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          // Failure in execution
          EXPECT_EQ(resp.status().code(), absl::StatusCode::kResourceExhausted);
          failed_execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  // execute success
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    auto payload_size = 1024 * 800;
    execution_obj->input.push_back("\"" + std::to_string(payload_size) + "\"");

    status = roma_service.Execute(std::move(execution_obj),
                                  [&](absl::StatusOr<ResponseObject> resp) {
                                    EXPECT_TRUE(resp.ok());
                                    retry_success_execute_finished.Notify();
                                  });
    EXPECT_TRUE(status.ok());
  }

  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(success_execute_finished.WaitForNotificationWithTimeout(
      absl::Seconds(10)));
  ASSERT_TRUE(failed_execute_finished.WaitForNotificationWithTimeout(
      absl::Seconds(10)));
  retry_success_execute_finished.WaitForNotificationWithTimeout(
      absl::Seconds(10));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

}  // namespace google::scp::roma::test
