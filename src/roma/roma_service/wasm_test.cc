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
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/log/scoped_mock_log.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"
#include "src/roma/wasm/testing_utils.h"

using google::scp::roma::sandbox::roma_service::RomaService;
using google::scp::roma::wasm::testing::WasmTestingUtils;
using ::testing::HasSubstr;
using ::testing::StrEq;

namespace google::scp::roma::test {
namespace {
constexpr auto kTimeout = absl::Seconds(10);
const std::vector<uint8_t> kWasmBin = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
    0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
    0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
    0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b,
};

TEST(WasmTest, CanExecuteWasmCode) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "src/roma/testing/"
      "cpp_wasm_string_in_string_out_example/"
      "string_in_string_out.wasm");
  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .wasm = std::string(reinterpret_cast<char*>(wasm_bin.data()),
                            wasm_bin.size()),
    });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response_status = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(kTimeout));
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
    ASSERT_TRUE(execute_finished.WaitForNotificationWithTimeout(kTimeout));
    ASSERT_TRUE(response_status.ok());
  }

  EXPECT_THAT(result, StrEq(R"("Foobar Hello World from WASM")"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(WasmTest, ReportsWasmStacktrace) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  absl::Notification load_finished;
  absl::Notification execute_finished;

  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "src/roma/testing/"
      "cpp_wasm_erroneous_code_example/"
      "erroneous_code.wasm");
  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .wasm = std::string(reinterpret_cast<char*>(wasm_bin.data()),
                            wasm_bin.size()),
    });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response_status = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(kTimeout));
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
                               execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(execute_finished.WaitForNotificationWithTimeout(kTimeout));
    EXPECT_EQ(response_status.code(), absl::StatusCode::kInternal);
    EXPECT_THAT(
        response_status.message(),
        // Since abort() causes the code to terminate
        // unexpectedly, it throws a runtime error: unreachable.
        // https://developer.mozilla.org/en-US/docs/WebAssembly/Reference/Control_flow/unreachable
        HasSubstr("Uncaught RuntimeError: unreachable"));
  }

  ASSERT_TRUE(roma_service.Stop().ok());
}

void LoggingFunction(absl::LogSeverity severity,
                     absl::flat_hash_map<std::string, std::string> metadata,
                     std::string_view msg) {
  LOG(LEVEL(severity)) << msg;
}

TEST(WasmTest, CanLogFromInlineWasmCode) {
  Config config;
  config.number_of_workers = 2;
  config.SetLoggingFunction(LoggingFunction);
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, testing::_, "LOG_STRING"));
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_, "ERR_STRING"));
  log.StartCapturingLogs();
  {
    const std::string inline_wasm_js = WasmTestingUtils::LoadJsWithWasmFile(
        "src/roma/testing/cpp_wasm_hello_world_with_logging_example/"
        "cpp_wasm_hello_world_with_logging_example_generated.js");

    const std::string udf = R"(
      async function HandleRequest(input, log_input, err_input) {
        console.log("JS HandleRequest START");
        const module = await getModule();
        console.log("WASM loaded");

        const result = module.HelloClass.SayHello(input, log_input, err_input);
        console.log("C++ result: " + result);
        return result;
      }
    )";

    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = absl::StrCat(inline_wasm_js, udf),
    });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response_status = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(kTimeout));
    ASSERT_TRUE(response_status.ok());
  }

  {
    const std::vector<std::string> inputs = {
        R"("Foobar")",
        R"("LOG_STRING")",
        R"("ERR_STRING")",
    };
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "HandleRequest",
            .input = inputs,
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
    ASSERT_TRUE(execute_finished.WaitForNotificationWithTimeout(kTimeout));
    ASSERT_TRUE(response_status.ok());
  }

  EXPECT_THAT(result, StrEq(R"("Hello from C++! Input: Foobar")"));

  ASSERT_TRUE(roma_service.Stop().ok());
  log.StopCapturingLogs();
}

TEST(WasmTest, CanExecuteJsWithWasmCode) {
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
          const module = new WebAssembly.Module(addModule);
          const instance = new WebAssembly.Instance(module);
          function hello_js(a, b) {
            return instance.exports.add(a, b);
          }
    )JS_CODE",
        .wasm_bin = kWasmBin,
        .tags = {{std::string{kWasmCodeArrayName}, "addModule"}},
    });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response_status = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(kTimeout));
    ASSERT_TRUE(response_status.ok());
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "hello_js",
            .input = {"1", "2"},
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
    ASSERT_TRUE(execute_finished.WaitForNotificationWithTimeout(kTimeout));
    ASSERT_TRUE(response_status.ok());
  }

  EXPECT_THAT(result, StrEq("3"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(WasmTest, LoadJSWithWasmCodeShouldFailOnInvalidRequest) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  absl::Notification load_finished1;
  absl::Notification load_finished2;

  constexpr std::string_view js_code = R"JS_CODE(
          const module = new WebAssembly.Module(addModule);
          const instance = new WebAssembly.Instance(module);
          function hello_js(a, b) {
            return instance.exports.add(a, b);
          }
  )JS_CODE";
  // Passing both the wasm and wasm_bin
  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = std::string(js_code),
        .wasm = "test",
        .wasm_bin = kWasmBin,
        .tags = {{std::string{kWasmCodeArrayName}, "addModule"}},
    });

    const auto status = roma_service.LoadCodeObj(
        std::move(code_obj), [&](absl::StatusOr<ResponseObject> resp) {});
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  }

  // Missing JS code
  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .wasm_bin = kWasmBin,
        .tags = {{std::string{kWasmCodeArrayName}, "addModule"}},
    });

    const auto status = roma_service.LoadCodeObj(
        std::move(code_obj), [&](absl::StatusOr<ResponseObject> resp) {});
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  }

  // Missing wasm code array name tag
  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = std::string(js_code),
        .wasm_bin = kWasmBin,
    });

    const auto status = roma_service.LoadCodeObj(
        std::move(code_obj), [&](absl::StatusOr<ResponseObject> resp) {});
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  }

  // Missing wasm_bin
  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = std::string(js_code),
        .tags = {{std::string{kWasmCodeArrayName}, "addModule"}},
    });

    const auto status = roma_service.LoadCodeObj(
        std::move(code_obj), [&](absl::StatusOr<ResponseObject> resp) {});
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  }

  // Wrong wasm array name tag
  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = std::string(js_code),
        .wasm_bin = kWasmBin,
        .tags = {{std::string{kWasmCodeArrayName}, "wrongName"}},
    });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response_status = resp.status();
                                   load_finished1.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(load_finished1.WaitForNotificationWithTimeout(kTimeout));
    EXPECT_FALSE(response_status.ok());
  }

  // Invalid wasm code array
  {
    const std::vector<uint8_t> invalid_wasm_bin{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07,
        0x01, 0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a,
        0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b,
    };
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = std::string(js_code),
        .wasm_bin = invalid_wasm_bin,
        .tags = {{std::string{kWasmCodeArrayName}, "addModule"}},
    });

    absl::Status response_status;
    EXPECT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response_status = resp.status();
                                   load_finished2.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(load_finished2.WaitForNotificationWithTimeout(kTimeout));
    EXPECT_FALSE(response_status.ok());
  }

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(WasmTest, CanExecuteJSWithWasmCodeWithStandaloneJS) {
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
        .js = "hello_js = (a, b) => a + b;",
        .wasm_bin = kWasmBin,
        .tags = {{std::string{kWasmCodeArrayName}, "addModule"}},
    });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response_status = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(kTimeout));
    ASSERT_TRUE(response_status.ok());
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "hello_js",
            .input = {"1", "2"},
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
    ASSERT_TRUE(execute_finished.WaitForNotificationWithTimeout(kTimeout));
    ASSERT_TRUE(response_status.ok());
  }

  EXPECT_THAT(result, StrEq("3"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

}  // namespace
}  // namespace google::scp::roma::test
