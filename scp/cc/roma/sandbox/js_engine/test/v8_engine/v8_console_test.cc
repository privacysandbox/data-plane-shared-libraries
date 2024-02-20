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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "include/v8.h"
#include "scp/cc/core/test/utils/auto_init_run_stop.h"
#include "scp/cc/public/core/test/interface/execution_result_matchers.h"
#include "scp/cc/roma/sandbox/js_engine/src/v8_engine/v8_isolate_function_binding.h"
#include "scp/cc/roma/sandbox/js_engine/src/v8_engine/v8_js_engine.h"
#include "scp/cc/roma/sandbox/native_function_binding/src/native_function_invoker.h"
#include "scp/cc/roma/sandbox/native_function_binding/src/rpc_wrapper.pb.h"

using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::roma::proto::RpcWrapper;

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

namespace google::scp::roma::sandbox::js_engine::test {
class V8ConsoleTest : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    js_engine::v8_js_engine::V8JsEngine engine;
    engine.OneTimeSetup();
  }
};

class NativeFunctionInvokerMock
    : public native_function_binding::NativeFunctionInvoker {
 public:
  MOCK_METHOD(absl::Status, Invoke, (RpcWrapper&), (noexcept, override));

  virtual ~NativeFunctionInvokerMock() = default;
};

TEST_F(V8ConsoleTest, ConsoleFunctionsInvokeRPC) {
  auto function_invoker = std::make_unique<NativeFunctionInvokerMock>();
  EXPECT_CALL(*function_invoker, Invoke(_))
      .Times(3)
      .WillOnce(Return(absl::OkStatus()));

  std::vector<std::string> function_names;
  auto visitor = std::make_unique<v8_js_engine::V8IsolateFunctionBinding>(
      function_names, std::move(function_invoker));

  js_engine::v8_js_engine::V8JsEngine js_engine(std::move(visitor));
  js_engine.Run();

  auto result_or = js_engine.CompileAndRunJs(
      R"(function func() {
        console.log("Hello World");
        console.warn("Hello World");
        console.log("Hello World");
        return "";
      })",
      "func", {}, {});
  ASSERT_SUCCESS(result_or.result());
  js_engine.Stop();
}

}  // namespace google::scp::roma::sandbox::js_engine::test
