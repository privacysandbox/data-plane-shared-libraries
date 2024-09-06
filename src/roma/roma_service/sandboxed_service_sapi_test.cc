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

#include <chrono>
#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/config/config.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/interface/roma.h"
#include "src/roma/native_function_grpc_server/proto/test_host_service_native_request_handler.h"
#include "src/roma/native_function_grpc_server/test_request_handlers.h"
#include "src/roma/roma_service/roma_service.h"
#include "src/util/duration.h"

using google::scp::roma::FunctionBindingPayload;
using google::scp::roma::sandbox::roma_service::kMinWorkerVirtualMemoryMB;
using google::scp::roma::sandbox::roma_service::RomaService;
using ::testing::_;
using ::testing::AnyOf;
using ::testing::DoubleNear;
using ::testing::Gt;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::StrEq;

namespace google::scp::roma::test {
namespace {

void EchoFunction(FunctionBindingPayload<>& wrapper) {
  wrapper.io_proto.set_output_string(wrapper.io_proto.input_string());
}

TEST(SandboxedServiceSapiTest,
     ShouldRespectJsHeapLimitsAndContinueWorkingAfterWorkerRestart) {
  Config config;
  // Only one worker so we can make sure it's actually restarted.
  config.number_of_workers = 1;
  // Too large an allocation will cause the worker to crash and be restarted
  // since we're giving it a max of 15 MB of heap for JS execution.
  config.ConfigureJsEngineResourceConstraints(1 /*initial_heap_size_in_mb*/,
                                              15 /*maximum_heap_size_in_mb*/);
  // We register a hook to make sure it continues to work when the worker is
  // restarted
  config.RegisterFunctionBinding(
      std::make_unique<FunctionBindingObjectV2<>>(FunctionBindingObjectV2<>{
          .function_name = "echo_function",
          .function = EchoFunction,
      }));

  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        // Dummy code to allocate memory based on input
        .js = R"(
        function Handler(input) {
          const bigObject = [];
          for (let i = 0; i < 1024*512*Number(input); i++) {
            var person = {
            name: 'test',
            age: 24,
            };
            bigObject.push(person);
          }
          return 233;
        }
      )",
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
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo2",
        .version_string = "v2",
        // Dummy code to exercise binding
        .js = R"(
        function Handler(input) {
          return echo_function(input);
        }
      )",
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
    absl::Notification execute_finished;
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            // Large input which should fail
            .input = {R"("10")"},
        });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response_status = resp.status();
                               execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    EXPECT_FALSE(response_status.ok());
    EXPECT_THAT(response_status.message(),
                StrEq("Sandbox worker crashed during "
                      "execution of request."));
  }

  {
    absl::Notification execute_finished;
    std::string result;

    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            // Small input which should work
            .input = {R"("1")"},
        });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response_status = resp.status();
                               if (resp.ok()) {
                                 result = std::move(resp->resp);
                               }
                               execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    EXPECT_TRUE(response_status.ok());
    EXPECT_THAT(result, StrEq("233"));
  }

  {
    absl::Notification execute_finished;
    std::string result;

    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v2",
            .handler_name = "Handler",
            // Small input which should work
            .input = {R"("Hello, World!")"},
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
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    EXPECT_TRUE(response_status.ok());
    EXPECT_THAT(result, StrEq(R"("Hello, World!")"));
  }

  EXPECT_TRUE(roma_service.Stop().ok());
}

}  // namespace
}  // namespace google::scp::roma::test
