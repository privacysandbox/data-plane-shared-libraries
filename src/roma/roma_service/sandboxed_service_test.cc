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

#include "absl/log/scoped_mock_log.h"
#include "absl/strings/match.h"
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

TEST(SandboxedServiceTest, InitStop) {
  Config config;
  config.number_of_workers = 2;

  RomaService roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());
  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, CanSetV8FlagsFromConfig) {
  Config config;
  std::vector<std::string>& v8_flags = config.SetV8Flags();
  v8_flags.push_back("--no-turbofan");
  config.number_of_workers = 2;

  RomaService roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());
  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest,
     ShouldFailToInitializeIfVirtualMemoryCapIsTooLittle) {
  Config config;
  config.number_of_workers = 2;
  config.max_worker_virtual_memory_mb = 10;

  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(
      status.message(),
      StrEq(absl::StrCat(
          "Roma startup failed due to insufficient address space for workers. "
          "Please increase config.max_worker_virtual_memory_mb above ",
          kMinWorkerVirtualMemoryMB, " MB. Current value is ",
          config.max_worker_virtual_memory_mb, " MB.")));

  ASSERT_TRUE(roma_service.Stop().ok());
}

// TODO: b/354030982 - Re-enable when setting virtual memory cap leads to
// deterministic behavior
TEST(SandboxedServiceTest,
     DISABLED_CanInitializeWithAppropriateVirtualMemoryCap) {
  Config config;
  config.number_of_workers = 2;
  config.max_worker_virtual_memory_mb = kMinWorkerVirtualMemoryMB;

  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());
  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, StopGracefullyWithPendingLoads) {
  RomaService<> roma_service(Config{});
  ASSERT_TRUE(roma_service.Init().ok());
  for (int i = 0; i < 100; ++i) {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = absl::StrCat("foo", i),
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE",
    });
    ASSERT_TRUE(
        roma_service.LoadCodeObj(std::move(code_obj), [](auto resp) {}).ok());
  }
  ASSERT_TRUE(roma_service.Stop().ok());
}

template <typename T>
std::string ProtoToBytesStr(const T& request) {
  std::string str = request.SerializeAsString();
  const uint8_t* byte_array = reinterpret_cast<const uint8_t*>(str.c_str());

  return absl::StrCat(
      "\"", absl::StrJoin(byte_array, byte_array + str.size(), " "), "\"");
}

TEST(SandboxedServiceTest, ProtobufCanBeSentRecievedAsBytes) {
  Config<std::string> config;
  config.number_of_workers = 2;
  config.enable_native_function_grpc_server = true;
  config.RegisterRpcHandler(
      "TestHostServer.NativeMethod",
      privacy_sandbox::test_host_server::NativeMethodHandler<std::string>());

  RomaService<std::string> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
        function ArrayStrToString(arrayStr) {
            const arr = arrayStr.split(" ");
            let string = "";
            for (let i = 0; i < arr.length; i++) {
                string += String.fromCharCode(parseInt(arr[i], 10));
            }
            return string;
        }

        function Handler(input) {
          return TestHostServer.NativeMethod(ArrayStrToString(input));
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
    // Convert proto to string representation of byte array and send to UDF.
    // Stand-in from UDF sending proto encoded as Uint8Array of bytes.
    privacy_sandbox::test_host_server::NativeMethodRequest request;
    request.set_input("Hello ");
    std::string request_bytes = ProtoToBytesStr(request);

    auto execution_obj = std::make_unique<InvocationStrRequest<std::string>>(
        InvocationStrRequest<std::string>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {request_bytes},
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
    ASSERT_TRUE(response_status.ok());
  }

  // result is a JSON representation of a string-serialized proto. Construct a
  // temporary JSON object to remove JSON-escaped characters. Necessary because
  // nlohmann can only parse JSON objects.
  std::string jsonStr = R"({"result": )" + result + "}";
  nlohmann::json j = nlohmann::json::parse(jsonStr);
  // Extract the string value from the JSON
  result = j["result"];

  privacy_sandbox::test_host_server::NativeMethodResponse response;
  response.ParseFromString(result);

  EXPECT_THAT(response.output(), StrEq("Hello World. From NativeMethod"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, ExecuteCode) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

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
                               if (resp.ok()) {
                                 result = std::move(resp->resp);
                               }
                               execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(response_status.ok());
  }

  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, CanCancelPendingRequests) {
  Config config;
  // Both requests are handled by the same worker
  config.number_of_workers = 1;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_hang_udf_finished;
  absl::Notification load_cancel_udf_finished;
  absl::Notification execute_hang_udf_finished;
  absl::Notification execute_cancel_udf_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Hang() {
      const startTime = Date.now();
      while (Date.now() - startTime < 1000) {}
    }
  )JS_CODE",
    });

    EXPECT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   EXPECT_TRUE(resp.ok());
                                   load_hang_udf_finished.Notify();
                                 })
                    .ok());
  }

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v2",
        .js = R"JS_CODE(
    function Handler() { return "Hello world!"; }
  )JS_CODE",
    });

    EXPECT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   EXPECT_TRUE(resp.ok());
                                   load_cancel_udf_finished.Notify();
                                 })
                    .ok());
  }

  ASSERT_TRUE(
      load_hang_udf_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(load_cancel_udf_finished.WaitForNotificationWithTimeout(
      absl::Seconds(10)));
  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Hang",
            .input = {},
        });

    EXPECT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&execute_hang_udf_finished](
                                 absl::StatusOr<ResponseObject> resp) {
                               EXPECT_TRUE(resp.ok());
                               execute_hang_udf_finished.Notify();
                             })
                    .ok());
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v2",
            .handler_name = "Handler",
            .input = {},
        });

    // This Request will be pending until the worker finishes executing the
    // above (Hang) request
    auto execution_token = roma_service.Execute(
        std::move(execution_obj),
        [&execute_cancel_udf_finished](absl::StatusOr<ResponseObject> resp) {
          EXPECT_FALSE(resp.ok());
          EXPECT_EQ(resp.status().code(), absl::StatusCode::kCancelled);
          execute_cancel_udf_finished.Notify();
        });
    EXPECT_TRUE(execution_token.ok());
    // Cancel the request before it gets executed
    roma_service.Cancel(*execution_token);
  }

  ASSERT_TRUE(execute_hang_udf_finished.WaitForNotificationWithTimeout(
      absl::Seconds(10)));
  ASSERT_TRUE(execute_cancel_udf_finished.WaitForNotificationWithTimeout(
      absl::Seconds(10)));
  EXPECT_TRUE(roma_service.Stop().ok());
}

// Hang should timeout if invoked
void Hang(FunctionBindingPayload<>& wrapper) {
  absl::Duration sleep_duration;
  EXPECT_TRUE(absl::ParseDuration("10s", &sleep_duration));
  absl::SleepFor(sleep_duration);
}

TEST(SandboxedServiceTest, CanCancelCurrentlyExecutingRequest) {
  Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(
      std::make_unique<FunctionBindingObjectV2<>>(FunctionBindingObjectV2<>{
          .function_name = "Hang",
          .function = Hang,
      }));
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler() {
      const startTime = Date.now();
      while (Date.now() - startTime < 2000) {}
      Hang();
    }
  )JS_CODE",
    });

    EXPECT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   EXPECT_TRUE(resp.ok());
                                   load_finished.Notify();
                                 })
                    .ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {},
        });

    auto execution_token = roma_service.Execute(
        std::move(execution_obj),
        [&result, &execute_finished](absl::StatusOr<ResponseObject> resp) {
          EXPECT_FALSE(resp.ok());
          result = resp.status().message();
          execute_finished.Notify();
        });
    EXPECT_TRUE(execution_token.ok());
    // Sleep for 1s to allow request to start but not finish
    absl::SleepFor(absl::Seconds(1));
    roma_service.Cancel(*execution_token);
  }
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_THAT(
      result,
      HasSubstr("ROMA: Error while executing native function binding."));

  EXPECT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, CancellingRequestDuringCallbackIsNoOp) {
  Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(
      std::make_unique<FunctionBindingObjectV2<>>(FunctionBindingObjectV2<>{
          .function_name = "Hang",
          .function = Hang,
      }));
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;
  absl::Notification callback_started;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler() {
      return "Hello World";
    }
  )JS_CODE",
    });

    EXPECT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   EXPECT_TRUE(resp.ok());
                                   load_finished.Notify();
                                 })
                    .ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {},
        });

    absl::Status response_status;
    auto execution_token = roma_service.Execute(
        std::move(execution_obj),
        [&result, &execute_finished, &response_status,
         &callback_started](absl::StatusOr<ResponseObject> resp) {
          response_status = resp.status();
          callback_started.Notify();
          if (resp.ok()) {
            result = std::move(resp->resp);
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(response_status.ok());
    EXPECT_TRUE(execution_token.ok());
    ASSERT_TRUE(
        callback_started.WaitForNotificationWithTimeout(absl::Seconds(10)));
    roma_service.Cancel(*execution_token);
  }
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_THAT(result, StrEq(R"("Hello World")"));

  EXPECT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, CancellingFinishedRequestIsNoOp) {
  Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(
      std::make_unique<FunctionBindingObjectV2<>>(FunctionBindingObjectV2<>{
          .function_name = "Hang",
          .function = Hang,
      }));
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler() {
      return "Hello World";
    }
  )JS_CODE",
    });

    absl::Status response_status;
    EXPECT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response_status = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(response_status.ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {},
        });

    absl::Status response_status;
    auto execution_token =
        roma_service.Execute(std::move(execution_obj),
                             [&result, &execute_finished, &response_status](
                                 absl::StatusOr<ResponseObject> resp) {
                               response_status = resp.status();
                               if (resp.ok()) {
                                 result = std::move(resp->resp);
                               }
                               execute_finished.Notify();
                             });
    EXPECT_TRUE(execution_token.ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    roma_service.Cancel(*execution_token);
  }

  EXPECT_THAT(result, StrEq(R"("Hello World")"));

  EXPECT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, CanRegisterGrpcServices) {
  Config config;
  config.number_of_workers = 2;
  config.enable_native_function_grpc_server = true;
  config.RegisterService(std::make_unique<grpc_server::AsyncMultiService>(),
                         grpc_server::TestMethod1Handler<DefaultMetadata>(),
                         grpc_server::TestMethod2Handler<DefaultMetadata>());

  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());
  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, ExecuteCodeWithStringViewInput) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto load_code_obj_request = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
            function Handler(input) { return "Hello world! " + JSON.stringify(input);
          }
        )JS_CODE",
    });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .LoadCodeObj(std::move(load_code_obj_request),
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
    std::string_view input_str_view{R"("Foobar")"};
    auto execute_request =
        std::make_unique<InvocationStrViewRequest<>>(InvocationStrViewRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {input_str_view},
        });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .Execute(std::move(execute_request),
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
    ASSERT_TRUE(response_status.ok());
  }

  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, ShouldFailWithInvalidHandlerName) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;
  absl::Notification failed_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
      function Handler(input) {
        return "Hello world! " + JSON.stringify(input);
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
            .input = {R"("Foobar")"},
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
    ASSERT_TRUE(response_status.ok());
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "WrongHandler",
            .input = {R"("Foobar")"},
        });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response_status = resp.status();
                               failed_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(
        failed_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    EXPECT_FALSE(response_status.ok());
    EXPECT_EQ(response_status.code(), absl::StatusCode::kInternal);
  }

  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, ExecuteCodeWithEmptyId) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
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

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("Foobar")"},
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
    ASSERT_TRUE(response_status.ok());
  }

  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, ShouldAllowEmptyInputs) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(arg1, arg2) { return arg1; }
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
    ASSERT_TRUE(response_status.ok());
  }

  EXPECT_THAT(result, StrEq("undefined"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, ShouldGetIdInResponse) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "my_cool_id",
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
                                   if (resp.ok()) {
                                     result = resp->id;
                                   }
                                   load_finished.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(response_status.ok());
    EXPECT_THAT(result, StrEq("my_cool_id"));
  }

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
                               if (resp.ok()) {
                                 result = std::move(resp->resp);
                               }
                               execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(response_status.ok());
  }

  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest,
     ShouldReturnWithVersionNotFoundWhenExecutingAVersionThatHasNotBeenLoaded) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  // We don't load any code, just try to execute some version
  absl::Notification execute_finished;

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
                               execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    // Execute should fail.
    EXPECT_FALSE(response_status.ok());
  }

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, CanRunAsyncJsCode) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
      function sleep(milliseconds) {
        const date = Date.now();
        let currentDate = null;
        do {
          currentDate = Date.now();
        } while (currentDate - date < milliseconds);
      }

      function multiplePromises() {
        const p1 = Promise.resolve("some");
        const p2 = "cool";
        const p3 = new Promise((resolve, reject) => {
          sleep(1000);
          resolve("string1");
        });
        const p4 = new Promise((resolve, reject) => {
          sleep(200);
          resolve("string2");
        });

        return Promise.all([p1, p2, p3, p4]).then((values) => {
          return values;
        });
      }

      async function Handler() {
          const result = await multiplePromises();
          return result.join(" ");
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
    ASSERT_TRUE(response_status.ok());
  }

  EXPECT_THAT(result, StrEq(R"("some cool string1 string2")"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, BatchExecute) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::vector<absl::StatusOr<ResponseObject>> batch_responses;
  int res_count = 0;
  constexpr size_t kBatchSize = 5;
  absl::Notification load_finished;
  absl::Notification execute_finished;
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

  {
    auto execution_obj = InvocationStrRequest<>({
        .id = "foo",
        .version_string = "v1",
        .handler_name = "Handler",
        .input = {R"("Foobar")"},
    });

    std::vector<InvocationStrRequest<>> batch(kBatchSize, execution_obj);
    ASSERT_TRUE(
        roma_service
            .BatchExecute(batch,
                          [&](const std::vector<absl::StatusOr<ResponseObject>>&
                                  batch_resp) {
                            batch_responses = batch_resp;
                            res_count = batch_resp.size();
                            execute_finished.Notify();
                          })
            .ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  }

  for (auto resp : batch_responses) {
    EXPECT_TRUE(resp.ok());
    EXPECT_THAT(resp->resp, StrEq(R"("Hello world! \"Foobar\"")"));
  }
  EXPECT_EQ(res_count, kBatchSize);

  ASSERT_TRUE(roma_service.Stop().ok());
}

void CallbackFunc(FunctionBindingPayload<>& wrapper) {
  wrapper.io_proto.set_output_string("Hello world!");
}

TEST(SandboxedServiceTest, DISABLED_CanReuseRequestForMultipleBatches) {
  Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(
      std::make_unique<FunctionBindingObjectV2<>>(FunctionBindingObjectV2<>{
          .function_name = "callback",
          .function = CallbackFunc,
      }));
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::vector<absl::StatusOr<ResponseObject>> batch_responses;
  int res_count = 0;
  constexpr size_t kBatchSize = 5;
  absl::Notification load_finished;
  absl::Notification execute_batch_1_finished;
  absl::Notification execute_batch_2_finished;
  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler() { return callback();
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
    // First batch will be executed, as a side effect, each request will have a
    // {kRequestUuid, uuid_str} pair in its tags.
    auto execution_obj = InvocationStrRequest<>({
        .id = "foo",
        .version_string = "v1",
        .handler_name = "Handler",
    });

    std::vector<InvocationStrRequest<>> batch(kBatchSize, execution_obj);
    ASSERT_TRUE(
        roma_service
            .BatchExecute(
                batch,
                [&](const std::vector<absl::StatusOr<ResponseObject>>&
                        batch_resp) { execute_batch_1_finished.Notify(); })
            .ok());
    ASSERT_TRUE(execute_batch_1_finished.WaitForNotificationWithTimeout(
        absl::Seconds(10)));

    // The second batch should still be able to be executed, ensuring the
    // {kRequestUuid, uuid_str} pair in each request's tags is updated.
    ASSERT_TRUE(
        roma_service
            .BatchExecute(batch,
                          [&](const std::vector<absl::StatusOr<ResponseObject>>&
                                  batch_resp) {
                            batch_responses = batch_resp;
                            res_count = batch_resp.size();
                            execute_batch_2_finished.Notify();
                          })
            .ok());

    ASSERT_TRUE(execute_batch_2_finished.WaitForNotificationWithTimeout(
        absl::Seconds(10)));
  }

  for (auto resp : batch_responses) {
    EXPECT_TRUE(resp.ok());
    EXPECT_THAT(resp->resp, StrEq(R"("Hello world!")"));
  }
  EXPECT_EQ(res_count, kBatchSize);

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest,
     BatchExecuteShouldExecuteAllRequestsEvenWithSmallQueues) {
  Config config;
  // Queue of size one and 10 workers. Incoming work should block while
  // workers are busy and can't pick up items.
  config.worker_queue_max_items = 1;
  config.number_of_workers = 10;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  absl::Mutex mu;
  std::vector<absl::StatusOr<ResponseObject>> batch_responses;
  int res_count = 0;
  // Large batch
  constexpr size_t kBatchSize = 100;
  absl::Notification load_finished;
  absl::Notification execute_finished;
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

  {
    auto execution_obj = InvocationStrRequest<>({
        .id = "foo",
        .version_string = "v1",
        .handler_name = "Handler",
        .input = {R"("Foobar")"},
    });

    std::vector<InvocationStrRequest<>> batch(kBatchSize, execution_obj);

    auto status = absl::InternalError("fail");
    while (!status.ok()) {
      status = roma_service.BatchExecute(
          batch,
          [&](const std::vector<absl::StatusOr<ResponseObject>>& batch_resp) {
            batch_responses = batch_resp;
            res_count = batch_resp.size();
            execute_finished.Notify();
          });
    }
    EXPECT_TRUE(status.ok());
  }

  for (auto resp : batch_responses) {
    EXPECT_TRUE(resp.ok());
    EXPECT_THAT(resp->resp, StrEq(R"("Hello world! \"Foobar\"")"));
  }

  execute_finished.WaitForNotification();
  EXPECT_EQ(res_count, kBatchSize);

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, MultiThreadedBatchExecuteSmallQueue) {
  Config config;
  config.worker_queue_max_items = 1;
  config.number_of_workers = 10;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());
  {
    absl::Notification load_finished;
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

  absl::Mutex res_count_mu;
  int res_count = 0;

  constexpr int kNumThreads = 10;
  constexpr size_t kBatchSize = 100;
  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; i++) {
    threads.emplace_back([&, i]() {
      absl::Notification local_execute;
      InvocationStrRequest<> execution_obj{
          .id = "foo",
          .version_string = "v1",
          .handler_name = "Handler",
          .input = {absl::StrCat(R"(")", "Foobar", i, R"(")")},
      };

      std::vector<InvocationStrRequest<>> batch(kBatchSize, execution_obj);
      std::vector<absl::StatusOr<ResponseObject>> batch_responses;

      auto batch_callback =
          [&](const std::vector<absl::StatusOr<ResponseObject>>& batch_resp) {
            batch_responses = batch_resp;
            {
              absl::MutexLock lock(&res_count_mu);
              res_count += batch_resp.size();
            }
            local_execute.Notify();
          };
      while (!roma_service.BatchExecute(batch, batch_callback).ok()) {
      }

      // Thread cannot join until batch_callback is called.
      ASSERT_TRUE(
          local_execute.WaitForNotificationWithTimeout(absl::Seconds(10)));

      for (auto resp : batch_responses) {
        if (resp.ok()) {
          EXPECT_THAT(
              resp->resp,
              StrEq(absl::StrCat("\"Hello world! \\\"Foobar", i, "\\\"\"")));
        } else {
          ADD_FAILURE() << "resp is NOT OK.";
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }
  {
    absl::MutexLock lock(&res_count_mu);
    EXPECT_EQ(res_count, kBatchSize * kNumThreads);
  }

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, ExecuteCodeConcurrently) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  absl::Notification load_finished;
  size_t total_runs = 10;
  std::vector<std::string> results(total_runs);
  std::vector<absl::Notification> finished(total_runs);
  std::vector<absl::Status> response_statuses(total_runs);
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

  {
    for (auto i = 0u; i < total_runs; ++i) {
      auto code_obj =
          std::make_unique<InvocationSharedRequest<>>(InvocationSharedRequest<>{
              .id = "foo",
              .version_string = "v1",
              .handler_name = "Handler",
              .input = {std::make_shared<std::string>(
                  R"("Foobar)" + std::to_string(i) + R"(")")},
          });

      ASSERT_TRUE(roma_service
                      .Execute(std::move(code_obj),
                               [&, i](absl::StatusOr<ResponseObject> resp) {
                                 response_statuses[i] = resp.status();
                                 if (resp.ok()) {
                                   results[i] = resp->resp;
                                 }
                                 finished[i].Notify();
                               })
                      .ok());
    }
  }

  for (auto i = 0u; i < total_runs; ++i) {
    EXPECT_TRUE(finished[i].WaitForNotificationWithTimeout(absl::Seconds(30)));
    EXPECT_TRUE(response_statuses[i].ok());
    std::string expected_result =
        absl::StrCat(R"("Hello world! )", "\\\"Foobar", i, "\\\"\"");
    EXPECT_THAT(results[i], StrEq(expected_result));
  }

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, ShouldReturnCorrectErrorForDifferentException) {
  Config config;
  config.number_of_workers = 1;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_timeout;
  absl::Notification execute_failed;
  absl::Notification execute_success;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"""(
    function sleep(milliseconds) {
      const date = Date.now();
      let currentDate = null;
      do {
        currentDate = Date.now();
      } while (currentDate - date < milliseconds);
    }
    let x;
    function hello_js(input) {
        sleep(200);
        if (input === undefined) {
          return x.value;
        }
        return "Hello world!"
      }
    )""",
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

  // The execution should timeout as the kTimeoutDurationTag value is too small.
  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "hello_js",
            .tags = {{std::string(kTimeoutDurationTag), "100ms"}},
        });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response_status = resp.status();
                               execute_timeout.Notify();
                             })
                    .ok());
    ASSERT_TRUE(
        execute_timeout.WaitForNotificationWithTimeout(absl::Seconds(10)));
    EXPECT_EQ(response_status.code(), absl::StatusCode::kInternal);
  }

  // The execution should return invoking error as it try to get value from
  // undefined var.
  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "hello_js",
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

  // The execution should success.
  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "hello_js",
            .tags = {{std::string(kTimeoutDurationTag), "300ms"}},
            .input = {R"("0")"},
        });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response_status = resp.status();
                               if (response_status.ok()) {
                                 result = std::move(resp->resp);
                               }

                               execute_success.Notify();
                             })
                    .ok());
    ASSERT_TRUE(
        execute_success.WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(response_status.ok());
  }

  EXPECT_THAT(result, StrEq(R"("Hello world!")"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, ShouldGetMetricsInResponse) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

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

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("Foobar")"},
        });

    absl::Status response_status;
    absl::flat_hash_map<std::string, absl::Duration> metrics;
    ASSERT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response_status = resp.status();
                               if (resp.ok()) {
                                 result = std::move(resp->resp);
                               }

                               metrics = resp->metrics;

                               std::cout << "Metrics:" << std::endl;
                               for (const auto& pair : resp->metrics) {
                                 std::cout << pair.first << ": " << pair.second
                                           << std::endl;
                               }

                               execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(response_status.ok());

    EXPECT_GT(metrics["roma.metric.sandboxed_code_run_duration"],
              absl::Duration());
    EXPECT_GT(metrics["roma.metric.code_run_duration"], absl::Duration());
    EXPECT_GT(metrics["roma.metric.json_input_parsing_duration"],
              absl::Duration());
    EXPECT_GT(metrics["roma.metric.js_engine_handler_call_duration"],
              absl::Duration());
  }

  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, ShouldAllowLoadingVersionWhileDispatching) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;

  // Load version 1
  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) { return "Hello world1! " + JSON.stringify(input);
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
    ASSERT_TRUE(response_status.ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  }

  // Start a batch execution
  {
    absl::Notification execute_finished;
    {
      std::vector<InvocationStrRequest<>> batch;
      for (int i = 0; i < 50; i++) {
        InvocationStrRequest<> req{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("Foobar")"},
        };
        batch.push_back(req);
      }
      auto batch_result = roma_service.BatchExecute(
          batch,
          [&](const std::vector<absl::StatusOr<ResponseObject>>& batch_resp) {
            for (auto& resp : batch_resp) {
              EXPECT_TRUE(resp.ok());
              if (resp.ok()) {
                result = std::move(resp->resp);
              }
            }
            execute_finished.Notify();
          });
    }

    // Load version 2 while execution is happening
    absl::Notification load_finished;
    {
      auto code_obj = std::make_unique<CodeObject>(CodeObject{
          .id = "foo",
          .version_string = "v2",
          .js = R"JS_CODE(
    function Handler(input) { return "Hello world2! " + JSON.stringify(input);
    }
  )JS_CODE",
      });

      ASSERT_TRUE(roma_service
                      .LoadCodeObj(std::move(code_obj),
                                   [&](absl::StatusOr<ResponseObject> resp) {
                                     EXPECT_TRUE(resp.ok());
                                     load_finished.Notify();
                                   })
                      .ok());
    }
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  }

  EXPECT_THAT(result, StrEq(R"("Hello world1! \"Foobar\"")"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, ShouldTimeOutIfExecutionExceedsDeadline) {
  Config config;
  config.number_of_workers = 1;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;

  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        // Code to sleep for the number of milliseconds passed as input
        .js = R"JS_CODE(
    function sleep(milliseconds) {
      const date = Date.now();
      let currentDate = null;
      do {
        currentDate = Date.now();
      } while (currentDate - date < milliseconds);
    }

    function Handler(input) {
      sleep(parseInt(input));
      return "Hello world!";
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

  privacy_sandbox::server_common::Stopwatch timer;

  {
    absl::Notification execute_finished;
    // Should not timeout since we only sleep for 9 sec but the timeout is 10
    // sec.
    constexpr int kTimeoutInMs = 9000;
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .tags = {{std::string(kTimeoutDurationTag), "10000ms"}},
            .input = {absl::StrCat("\"", kTimeoutInMs, "\"")},
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
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(30)));
    ASSERT_TRUE(response_status.ok());

    auto elapsed_time_ms = absl::ToDoubleMilliseconds(timer.GetElapsedTime());
    // Should have elapsed more than 9sec.
    EXPECT_THAT(elapsed_time_ms,
                AnyOf(DoubleNear(kTimeoutInMs, /*max_abs_error=*/10),
                      Gt(kTimeoutInMs)));
    // But less than 10.
    EXPECT_LT(elapsed_time_ms, 10000);
    EXPECT_THAT(result, StrEq(R"("Hello world!")"));
  }

  result = "";
  timer.Reset();

  {
    absl::Notification execute_finished;
    // Should time out since we sleep for 11 which is longer than the 10
    // sec timeout.
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .tags = {{std::string(kTimeoutDurationTag), "10000ms"}},
            .input = {R"("11000")"},
        });

    ASSERT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               EXPECT_FALSE(resp.ok());
                               execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(30)));
  }

  auto elapsed_time_ms = absl::ToDoubleMilliseconds(timer.GetElapsedTime());
  // Should have elapsed more than 10sec since that's our
  // timeout.
  EXPECT_THAT(elapsed_time_ms,
              AnyOf(DoubleNear(10000, /*max_abs_error=*/10), Gt(10000)));
  // But less than 11
  EXPECT_LT(elapsed_time_ms, 11000);
  EXPECT_THAT(result, IsEmpty());

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, ShouldGetCompileErrorForBadJsCode) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  absl::Notification load_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        // Bad JS code.
        .js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
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
    EXPECT_FALSE(response_status.ok());
    EXPECT_EQ(response_status.code(), absl::StatusCode::kInternal);
  }

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, ShouldGetExecutionErrorWhenJsCodeThrowError) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  absl::Notification load_finished;
  absl::Notification execute_finished;
  absl::Notification execute_failed;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
      function Handler(input) {
        if (input === "0") {
          throw new Error('Yeah...Input cannot be 0!');
        }
        return "Hello world! " + JSON.stringify(input);
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
            .input = {"9000"},
        });

    absl::Status response_status;
    std::string result;
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
    ASSERT_TRUE(response_status.ok());
    EXPECT_THAT(result, StrEq(R"("Hello world! 9000")"));
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("0")"},
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
}

TEST(SandboxedServiceTest, StackTraceAvailableInResponse) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  absl::Notification load_finished;
  absl::Notification execute_failed;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
      function Handler() {
        throw new Error('foobar');
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
    EXPECT_TRUE(absl::StrContains(response_status.message(), "at Handler"));
  }

  ASSERT_TRUE(roma_service.Stop().ok());
}

void LoggingFunction(absl::LogSeverity severity, DefaultMetadata metadata,
                     std::string_view msg) {
  LOG(LEVEL(severity)) << msg;
}

TEST(SandboxedServiceTest, ShouldNotGetStackTraceWhenDisableStackTraceFlagSet) {
  Config config;
  config.number_of_workers = 2;
  config.disable_udf_stacktraces_in_response = true;
  config.SetLoggingFunction(LoggingFunction);
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  absl::Notification load_finished;
  absl::Notification execute_failed;

  absl::ScopedMockLog log;
  // disable_udf_stacktraces_in_response should not prevent stacktraces from
  // being logged.
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, _, HasSubstr("at Handler")));
  log.StartCapturingLogs();

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
      function Handler() {
        throw new Error('foobar');
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
    EXPECT_FALSE(absl::StrContains(response_status.message(), "at Handler"));
  }

  ASSERT_TRUE(roma_service.Stop().ok());
  log.StopCapturingLogs();
}

TEST(SandboxedServiceTest, ShouldGetExecutionErrorWhenJsCodeReturnUndefined) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  absl::Notification load_finished;
  absl::Notification execute_finished;
  absl::Notification execute_failed;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
      let x;
      function Handler(input) {
        if (input === "0") {
          return "Hello world! " + x.value;
        }
        return "Hello world! " + JSON.stringify(input);
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
            .input = {"9000"},
        });

    std::string result;
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
    ASSERT_TRUE(response_status.ok());
    EXPECT_THAT(result, StrEq(R"("Hello world! 9000")"));
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("0")"},
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
    EXPECT_FALSE(response_status.ok());
    EXPECT_EQ(response_status.code(), absl::StatusCode::kInternal);
  }

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, CanHandleMultipleInputs) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(arg1, arg2) {
      return arg1 + arg2;
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
            .input = {R"("Foobar1")", R"(" Barfoo2")"},
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
    ASSERT_TRUE(response_status.ok());
  }

  EXPECT_THAT(result, StrEq(R"("Foobar1 Barfoo2")"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, ErrorShouldBeExplicitWhenInputCannotBeParsed) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) {
      return input;
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
            // Not a JSON string
            .input = {"Foobar1"},
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
    EXPECT_EQ(response_status.code(), absl::StatusCode::kInternal);
  }

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest,
     ShouldGetErrorIfLoadFailsButExecutionIsSentForVersion) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;

  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        // Bad syntax so load should fail
        .js = R"JS_CODE(
    function Handler(input) { return "123
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
    // Load should have failed
    EXPECT_FALSE(response_status.ok());
  }

  {
    absl::Notification execute_finished;
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
                               execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    // Execution should fail since load didn't work
    // for this code version
    EXPECT_EQ(response_status.code(), absl::StatusCode::kInternal);
  }

  // Should be able to load same version
  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"(function Handler() { return "Hello there"; })",
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

  // Execution should work now
  {
    absl::Notification execute_finished;
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
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
    ASSERT_TRUE(response_status.ok());
  }

  EXPECT_THAT(result, StrEq(R"("Hello there")"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, ShouldBeAbleToOverwriteVersion) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;

  // Load v1
  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) { return "version 1"; }
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

  // Execute version 1
  {
    absl::Notification execute_finished;
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
                               if (resp.ok()) {
                                 result = std::move(resp->resp);
                               }

                               execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(response_status.ok());
    EXPECT_THAT(result, StrEq(R"("version 1")"));
  }

  // Should be able to load same version
  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler() { return "version 1 but updated";}
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

  // Execution should run the new version of the code
  {
    absl::Notification execute_finished;
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
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
    ASSERT_TRUE(response_status.ok());
    EXPECT_THAT(result, StrEq(R"("version 1 but updated")"));
  }

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, CompilationFailureReturnsDetailedError) {
  Config config;
  config.number_of_workers = 1;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;

  // Load v1
  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        // Since 'apples' is not defined, the statement 'console.log(apples)'
        // should cause a compilation error.
        .js = R"JS_CODE(console.log(apples)
function Handler(input) { return "version 1"; }
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
    EXPECT_FALSE(response_status.ok());
    EXPECT_EQ(response_status.code(), absl::StatusCode::kInternal);
    EXPECT_THAT(response_status.message(), HasSubstr("Uncaught ReferenceError: "
                                                     "apples is not defined"));
  }
  ASSERT_TRUE(roma_service.Stop().ok());
}
}  // namespace
}  // namespace google::scp::roma::test
