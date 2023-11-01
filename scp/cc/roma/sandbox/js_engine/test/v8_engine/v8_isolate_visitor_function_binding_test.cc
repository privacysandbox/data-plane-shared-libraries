/*
 * Copyright 2022 Google LLC
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

#include "roma/sandbox/js_engine/src/v8_engine/v8_isolate_visitor_function_binding.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "core/test/utils/auto_init_run_stop.h"
#include "include/v8.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "roma/sandbox/js_engine/src/v8_engine/v8_js_engine.h"
#include "roma/sandbox/native_function_binding/src/native_function_invoker.h"
#include "scp/cc/roma/interface/function_binding_io.pb.h"

using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::AutoInitRunStop;
using google::scp::roma::proto::FunctionBindingIoProto;

using ::testing::_;

namespace google::scp::roma::sandbox::js_engine::test {
class V8IsolateVisitorFunctionBindingTest : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    js_engine::v8_js_engine::V8JsEngine engine;
    engine.OneTimeSetup();
  }
};

class NativeFunctionInvokerMock
    : public native_function_binding::NativeFunctionInvoker {
 public:
  MOCK_METHOD(ExecutionResult, Invoke,
              (const std::string&, FunctionBindingIoProto&),
              (noexcept, override));

  virtual ~NativeFunctionInvokerMock() = default;
};

TEST_F(V8IsolateVisitorFunctionBindingTest,
       FunctionBecomesAvailableInJavascript) {
  auto function_invoker = std::make_shared<NativeFunctionInvokerMock>();
  std::vector<std::string> function_names = {"cool_func"};
  auto visitor =
      std::make_shared<v8_js_engine::V8IsolateVisitorFunctionBinding>(
          function_names, function_invoker);

  js_engine::v8_js_engine::V8JsEngine js_engine(visitor);
  AutoInitRunStop to_handle_engine(js_engine);

  EXPECT_CALL(*function_invoker, Invoke("cool_func", _))
      .WillOnce(testing::Return(SuccessExecutionResult()));

  auto result_or = js_engine.CompileAndRunJs(
      R"(function func() { cool_func(); return ""; })", "func", {}, {});
  EXPECT_SUCCESS(result_or.result());
  const auto& response_string = result_or->execution_response.response;
  EXPECT_THAT(response_string, testing::StrEq(R"("")"));
}

}  // namespace google::scp::roma::sandbox::js_engine::test
