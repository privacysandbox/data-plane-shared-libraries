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
#include "src/roma/sandbox/js_engine/v8_engine/v8_isolate_function_binding.h"
#include "src/roma/sandbox/js_engine/v8_engine/v8_js_engine.h"
#include "src/roma/sandbox/native_function_binding/native_function_invoker.h"
#include "src/roma/sandbox/native_function_binding/rpc_wrapper.pb.h"

using google::scp::roma::proto::RpcWrapper;

using google::scp::roma::sandbox::js_engine::v8_js_engine::V8JsEngine;
using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

namespace google::scp::roma::sandbox::js_engine::test {
class V8ConsoleTest : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    static constexpr bool skip_v8_cleanup = true;
    V8JsEngine(nullptr, skip_v8_cleanup).OneTimeSetup();
  }

  static void TearDownTestSuite() { V8JsEngine(nullptr); }
};

class NativeFunctionInvokerMock
    : public native_function_binding::NativeFunctionInvoker {
 public:
  MOCK_METHOD(absl::Status, Invoke, (RpcWrapper&), (override));

  virtual ~NativeFunctionInvokerMock() = default;
};

TEST_F(V8ConsoleTest, DISABLED_ConsoleFunctionsInvokeRPC) {
  auto function_invoker = std::make_unique<NativeFunctionInvokerMock>();
  EXPECT_CALL(*function_invoker, Invoke(_))
      .Times(3)
      .WillOnce(Return(absl::OkStatus()));

  std::vector<std::string> function_names;
  auto visitor = std::make_unique<v8_js_engine::V8IsolateFunctionBinding>(
      function_names, /*rpc_method_names=*/std::vector<std::string>(),
      std::move(function_invoker), /*server_address=*/"");

  static constexpr bool skip_v8_cleanup = true;
  V8JsEngine js_engine(std::move(visitor), skip_v8_cleanup);
  js_engine.Run();

  auto result_or = js_engine.CompileAndRunJs(
      R"(function func() {
        console.log("Hello World");
        console.warn("Hello World");
        console.log("Hello World");
        return "";
      })",
      "func", {}, {});
  ASSERT_TRUE(result_or.ok());
  js_engine.Stop();
}

}  // namespace google::scp::roma::sandbox::js_engine::test
