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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/js_engine/v8_engine/v8_js_engine.h"
#include "src/roma/sandbox/worker/worker.h"
#include "src/roma/wasm/testing_utils.h"

using google::scp::roma::kWasmCodeArrayName;
using google::scp::roma::sandbox::constants::kCodeVersion;
using google::scp::roma::sandbox::constants::kHandlerName;
using google::scp::roma::sandbox::constants::kRequestAction;
using google::scp::roma::sandbox::constants::kRequestActionExecute;
using google::scp::roma::sandbox::constants::kRequestActionLoad;
using google::scp::roma::sandbox::constants::kRequestType;
using google::scp::roma::sandbox::constants::kRequestTypeJavascript;
using google::scp::roma::sandbox::constants::kRequestTypeJavascriptWithWasm;
using google::scp::roma::sandbox::constants::kRequestTypeWasm;
using google::scp::roma::sandbox::js_engine::v8_js_engine::V8JsEngine;
using google::scp::roma::wasm::testing::WasmTestingUtils;
using ::testing::IsEmpty;
using ::testing::StrEq;

namespace google::scp::roma::sandbox::worker::test {
namespace {
constexpr std::array<uint8_t, 41> kWasmBin = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
    0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
    0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
    0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b,
};

class V8EngineWorkerTest : public ::testing::Test {
 public:
  static std::unique_ptr<V8JsEngine> CreateEngine() {
    static constexpr bool skip_v8_cleanup = true;
    return std::make_unique<V8JsEngine>(nullptr, skip_v8_cleanup);
  }

  static void SetUpTestSuite() { CreateEngine()->OneTimeSetup(); }

  static void TearDownTestSuite() { std::make_unique<V8JsEngine>(nullptr); }
};

TEST_F(V8EngineWorkerTest, CanRunJsCode) {
  Worker worker(CreateEngine(), /*require_preload=*/false);
  worker.Run();

  constexpr std::string_view js_code =
      R"(function hello_js() { return "Hello World!"; })";
  std::vector<std::string_view> input;
  const absl::flat_hash_map<std::string_view, std::string_view> metadata = {
      {kRequestType, kRequestTypeJavascript},
      {kHandlerName, "hello_js"},
      {kCodeVersion, "1"},
      {kRequestAction, kRequestActionExecute},
  };

  constexpr absl::Span<const uint8_t> empty_wasm;
  const auto response_or =
      worker.RunCode(std::string(js_code), input, metadata, empty_wasm);
  ASSERT_TRUE(response_or.ok());

  EXPECT_THAT(response_or->response, StrEq(R"("Hello World!")"));
  worker.Stop();
}

TEST_F(V8EngineWorkerTest, CanRunMultipleVersionsOfTheCode) {
  Worker worker(CreateEngine(), /*require_preload=*/true);
  worker.Run();

  // Load v1
  std::string js_code = R"(function hello_js() { return "Hello Version 1!"; })";
  std::vector<std::string_view> input;
  absl::flat_hash_map<std::string_view, std::string_view> metadata = {
      {kRequestType, kRequestTypeJavascript},
      {kCodeVersion, "1"},
      {kRequestAction, kRequestActionLoad},
  };

  constexpr absl::Span<const uint8_t> empty_wasm;
  auto response_or = worker.RunCode(js_code, input, metadata, empty_wasm);
  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response, IsEmpty());

  // Load v2
  js_code = R"(function hello_js() { return "Hello Version 2!"; })";
  metadata = {
      {kRequestType, kRequestTypeJavascript},
      {kCodeVersion, "2"},
      {kRequestAction, kRequestActionLoad},
  };

  response_or = worker.RunCode(js_code, input, metadata, empty_wasm);
  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response, IsEmpty());

  // Execute v1
  js_code = "";
  metadata = {
      {kRequestType, kRequestTypeJavascript},
      {kCodeVersion, "1"},
      {kRequestAction, kRequestActionExecute},
      {kHandlerName, "hello_js"},
  };

  response_or = worker.RunCode(js_code, input, metadata, empty_wasm);
  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response, StrEq(R"("Hello Version 1!")"));

  // Execute v2
  js_code = "";
  metadata = {
      {kRequestType, kRequestTypeJavascript},
      {kCodeVersion, "2"},
      {kRequestAction, kRequestActionExecute},
      {kHandlerName, "hello_js"},
  };

  response_or = worker.RunCode(js_code, input, metadata, empty_wasm);
  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response, StrEq(R"("Hello Version 2!")"));
  worker.Stop();
}

TEST_F(V8EngineWorkerTest, CanRunMultipleVersionsOfCompilationContexts) {
  Worker worker(CreateEngine(), /*require_preload=*/true);
  worker.Run();

  // Load v1
  std::string js_code = R"""(
          let bytes = new Uint8Array([
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
            0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
            0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
            0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b
          ]);
          let module = new WebAssembly.Module(bytes);
          let instance = new WebAssembly.Instance(module);
          function hello_js(a, b) {
            return instance.exports.add(a, b);
          }
        )""";
  std::vector<std::string_view> input;
  absl::flat_hash_map<std::string_view, std::string_view> metadata = {
      {kRequestType, kRequestTypeJavascript},
      {kCodeVersion, "1"},
      {kRequestAction, kRequestActionLoad},
  };

  constexpr absl::Span<const uint8_t> empty_wasm;
  auto response_or = worker.RunCode(js_code, input, metadata, empty_wasm);
  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response, IsEmpty());

  // Load v2
  js_code = R"""(
          let bytes = new Uint8Array([
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
            0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
            0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
            0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b
          ]);
          function hello_js(a, b) {
            var module = new WebAssembly.Module(bytes);
            var instance = new WebAssembly.Instance(module);
            return instance.exports.add(a, b);
          }
        )""";
  metadata = {
      {kRequestType, kRequestTypeJavascript},
      {kCodeVersion, "2"},
      {kRequestAction, kRequestActionLoad},
  };

  response_or = worker.RunCode(js_code, input, metadata, empty_wasm);
  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response, IsEmpty());

  // Execute v1
  {
    js_code = "";
    input = {"1", "2"};
    metadata = {
        {kRequestType, kRequestTypeJavascript},
        {kCodeVersion, "1"},
        {kRequestAction, kRequestActionExecute},
        {kHandlerName, "hello_js"},
    };

    response_or = worker.RunCode(js_code, input, metadata, empty_wasm);
    ASSERT_TRUE(response_or.ok());
    EXPECT_THAT(response_or->response, StrEq("3"));
  }

  // Execute v2
  {
    js_code = "";
    input = {"5", "7"};
    metadata = {
        {kRequestType, kRequestTypeJavascript},
        {kCodeVersion, "2"},
        {kRequestAction, kRequestActionExecute},
        {kHandlerName, "hello_js"},
    };

    response_or = worker.RunCode(js_code, input, metadata, empty_wasm);
    ASSERT_TRUE(response_or.ok());
    EXPECT_THAT(response_or->response, StrEq("12"));
  }
  worker.Stop();
}

TEST_F(V8EngineWorkerTest, ShouldBeAbleToOverwriteAVersionOfTheCode) {
  Worker worker(CreateEngine(), /*require_preload=*/true);
  worker.Run();

  // Load v1
  std::string js_code = R"(function hello_js() { return "Hello Version 1!"; })";
  std::vector<std::string_view> input;
  absl::flat_hash_map<std::string_view, std::string_view> metadata = {
      {kRequestType, kRequestTypeJavascript},
      {kCodeVersion, "1"},
      {kRequestAction, kRequestActionLoad},
  };
  constexpr absl::Span<const uint8_t> empty_wasm;
  auto response_or = worker.RunCode(js_code, input, metadata, empty_wasm);
  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response, IsEmpty());

  // Load v2
  js_code = R"(function hello_js() { return "Hello Version 2!"; })";
  metadata = {
      {kRequestType, kRequestTypeJavascript},
      {kCodeVersion, "2"},
      {kRequestAction, kRequestActionLoad},
  };

  response_or = worker.RunCode(js_code, input, metadata, empty_wasm);
  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response, IsEmpty());

  // Execute v1
  js_code = "";
  metadata = {
      {kRequestType, kRequestTypeJavascript},
      {kCodeVersion, "1"},
      {kRequestAction, kRequestActionExecute},
      {kHandlerName, "hello_js"},
  };

  response_or = worker.RunCode(js_code, input, metadata, empty_wasm);
  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response, StrEq(R"("Hello Version 1!")"));

  // Execute v2
  js_code = "";
  metadata = {
      {kRequestType, kRequestTypeJavascript},
      {kCodeVersion, "2"},
      {kRequestAction, kRequestActionExecute},
      {kHandlerName, "hello_js"},
  };

  response_or = worker.RunCode(js_code, input, metadata, empty_wasm);
  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response, StrEq(R"("Hello Version 2!")"));

  // Load v2 updated (overwrite the version of the code)
  js_code = R"(function hello_js() { return "Hello Version 2 but Updated!"; })";
  metadata = {
      {kRequestType, kRequestTypeJavascript},
      {kCodeVersion, "2"},
      {kRequestAction, kRequestActionLoad},
  };

  response_or = worker.RunCode(js_code, input, metadata, empty_wasm);
  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response, IsEmpty());

  // Execute v2 should result in updated version
  js_code = "";
  metadata = {
      {kRequestType, kRequestTypeJavascript},
      {kCodeVersion, "2"},
      {kRequestAction, kRequestActionExecute},
      {kHandlerName, "hello_js"},
  };

  response_or = worker.RunCode(js_code, input, metadata, empty_wasm);
  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response,
              StrEq(R"("Hello Version 2 but Updated!")"));

  // But Executing v1 should yield not change
  js_code = "";
  metadata = {
      {kRequestType, kRequestTypeJavascript},
      {kCodeVersion, "1"},
      {kRequestAction, kRequestActionExecute},
      {kHandlerName, "hello_js"},
  };

  response_or = worker.RunCode(js_code, input, metadata, empty_wasm);
  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response, StrEq(R"("Hello Version 1!")"));
  worker.Stop();
}

TEST_F(V8EngineWorkerTest, CanRunJsWithWasmCode) {
  Worker worker(CreateEngine(), /*require_preload=*/false);
  worker.Run();

  auto js_code = R"""(
          const module = new WebAssembly.Module(addModule);
          const instance = new WebAssembly.Instance(module);
          function hello_js(a, b) {
            return instance.exports.add(a, b);
          }
        )""";
  std::vector<std::string_view> input{"1", "2"};
  absl::flat_hash_map<std::string_view, std::string_view> metadata = {
      {kRequestType, kRequestTypeJavascriptWithWasm},
      {kHandlerName, "hello_js"},
      {kCodeVersion, "1"},
      {kRequestAction, kRequestActionExecute},
      {kWasmCodeArrayName, "addModule"},
  };

  auto wasm = absl::MakeConstSpan(kWasmBin);
  auto response_or = worker.RunCode(js_code, input, metadata, wasm);

  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response, StrEq("3"));
  worker.Stop();
}

TEST_F(V8EngineWorkerTest, JSWithWasmCanRunMultipleVersionsOfTheCode) {
  Worker worker(CreateEngine(), /*require_preload=*/true);
  worker.Run();
  std::vector<std::string_view> input;

  // Load v1
  auto js_code = R"""(
          const module = new WebAssembly.Module(addModule);
          const instance = new WebAssembly.Instance(module);
          function hello_js(a, b) {
            return instance.exports.add(a, b);
          }
        )""";
  absl::flat_hash_map<std::string_view, std::string_view> metadata = {
      {kRequestType, kRequestTypeJavascriptWithWasm},
      {kCodeVersion, "1"},
      {kRequestAction, kRequestActionLoad},
      {kWasmCodeArrayName, "addModule"},
  };

  auto wasm = absl::MakeConstSpan(kWasmBin);
  auto response_or = worker.RunCode(js_code, input, metadata, wasm);
  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response, IsEmpty());

  // Load v2
  js_code = R"""(
      const wasmImports = {
        wasi_snapshot_preview1: {
          proc_exit() {
            return;
          },
        },
      };
      const module = new WebAssembly.Module(testModule);
      const instance = new WebAssembly.Instance(module, wasmImports);
      function test_wasi(a) {
        return instance.exports.Handler(a);
      }
    )""";
  auto wasm_str = WasmTestingUtils::LoadWasmFile(
      "./src/roma/testing/cpp_wasi_dependency_example/wasi_dependency.wasm");
  std::vector<uint8_t> wasm_code(wasm_str.begin(), wasm_str.end());
  wasm = absl::Span<const uint8_t>(wasm_code);
  metadata = {
      {kRequestType, kRequestTypeJavascriptWithWasm},
      {kCodeVersion, "2"},
      {kRequestAction, kRequestActionLoad},
      {kWasmCodeArrayName, "testModule"},
  };

  response_or = worker.RunCode(js_code, input, metadata, wasm);
  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response, IsEmpty());

  // Execute v1
  js_code = "";

  metadata = {
      {kRequestType, kRequestTypeJavascriptWithWasm},
      {kCodeVersion, "1"},
      {kRequestAction, kRequestActionExecute},
      {kHandlerName, "hello_js"},
  };

  input = {"1", "2"};
  response_or = worker.RunCode(js_code, input, metadata, wasm);
  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response, StrEq("3"));

  // Execute v2
  js_code = "";
  metadata = {
      {kRequestType, kRequestTypeJavascriptWithWasm},
      {kCodeVersion, "2"},
      {kRequestAction, kRequestActionExecute},
      {kHandlerName, "test_wasi"},
  };

  input = {"1"};
  response_or = worker.RunCode(js_code, input, metadata, wasm);
  ASSERT_TRUE(response_or.ok());
  EXPECT_THAT(response_or->response, StrEq("0"));
  worker.Stop();
}
}  // namespace
}  // namespace google::scp::roma::sandbox::worker::test
