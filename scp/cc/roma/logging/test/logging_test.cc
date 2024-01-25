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
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "core/test/utils/auto_init_run_stop.h"
#include "roma/interface/roma.h"
#include "roma/sandbox/js_engine/src/v8_engine/v8_isolate_function_binding.h"
#include "roma/sandbox/js_engine/src/v8_engine/v8_js_engine.h"
#include "roma/sandbox/native_function_binding/src/native_function_invoker.h"

using google::scp::roma::sandbox::js_engine::v8_js_engine::
    V8IsolateFunctionBinding;
using google::scp::roma::sandbox::js_engine::v8_js_engine::V8JsEngine;
using google::scp::roma::sandbox::native_function_binding::
    NativeFunctionInvoker;

using ::testing::_;

namespace google::scp::roma::test {
class LoggingTest : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    V8JsEngine engine;
    engine.OneTimeSetup();
  }
};

class NativeFunctionInvokerMock : public NativeFunctionInvoker {
 public:
  MOCK_METHOD(absl::Status, Invoke, (proto::RpcWrapper&), (noexcept, override));

  virtual ~NativeFunctionInvokerMock() = default;
};

TEST_F(LoggingTest, ShouldCallRegisteredLogFunctionBindings) {
  const std::vector<std::string> kOutputs{
      R"("str1")",
      R"("str2")",
      R"("str3")",
  };

  const auto trim_first_last_char = [](const std::string& str) {
    return str.substr(1, str.length() - 2);
  };

  absl::ScopedMockLog log;
  EXPECT_CALL(
      log, Log(absl::LogSeverity::kInfo, _, trim_first_last_char(kOutputs[0])));
  EXPECT_CALL(log, Log(absl::LogSeverity::kWarning, _,
                       trim_first_last_char(kOutputs[1])));
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, _,
                       trim_first_last_char(kOutputs[2])));

  EXPECT_CALL(
      log, Log(absl::LogSeverity::kInfo, _,
               absl::StrCat("metrics1: ", trim_first_last_char(kOutputs[0]))));
  EXPECT_CALL(
      log, Log(absl::LogSeverity::kInfo, _,
               absl::StrCat("metrics2: ", trim_first_last_char(kOutputs[1]))));
  EXPECT_CALL(
      log, Log(absl::LogSeverity::kInfo, _,
               absl::StrCat("metrics3: ", trim_first_last_char(kOutputs[2]))));
  log.StartCapturingLogs();

  auto function_invoker = std::make_unique<NativeFunctionInvokerMock>();
  std::vector<std::string> function_names = {};
  auto visitor = std::make_unique<V8IsolateFunctionBinding>(
      function_names, std::move(function_invoker));

  V8JsEngine engine(std::move(visitor));
  engine.Run();

  constexpr auto js_code = R"JS_CODE(
    function Handler(input1, input2, input3) {
      roma.log(input1);
      roma.warn(input2);
      roma.error(input3);
      roma.recordMetrics({
        "metrics1": input1,
        "metrics2": input2,
        "metrics3": input3,
      })
      return "Hello World";
    }
  )JS_CODE";

  auto response_or = engine.CompileAndRunJs(
      js_code, "Handler",
      std::vector<absl::string_view>(kOutputs.begin(), kOutputs.end()),
      {} /*metadata*/);

  ASSERT_SUCCESS(response_or.result());
  auto response_string = response_or->execution_response.response;
  EXPECT_THAT(response_string, testing::StrEq(R"("Hello World")"));

  log.StopCapturingLogs();
  engine.Stop();
}
}  // namespace google::scp::roma::test
