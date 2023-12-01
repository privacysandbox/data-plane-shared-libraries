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

#include "roma/worker/src/execution_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <v8-context.h>
#include <v8-initialization.h>
#include <v8-isolate.h>

#include <linux/limits.h>

#include <memory>
#include <string>
#include <vector>

#include "core/test/utils/auto_init_run_stop.h"
#include "include/libplatform/libplatform.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "roma/wasm/src/deserializer.h"
#include "roma/wasm/src/wasm_types.h"
#include "roma/wasm/test/testing_utils.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::SC_ROMA_V8_WORKER_BAD_HANDLER_NAME;
using google::scp::core::errors::SC_ROMA_V8_WORKER_BAD_INPUT_ARGS;
using google::scp::core::errors::SC_ROMA_V8_WORKER_CODE_COMPILE_FAILURE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_CODE_EXECUTION_FAILURE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_HANDLER_INVALID_FUNCTION;
using google::scp::core::errors::SC_ROMA_V8_WORKER_RESULT_PARSE_FAILURE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_WASM_COMPILE_FAILURE;
using google::scp::core::test::ResultIs;
using google::scp::roma::WasmDataType;
using google::scp::roma::wasm::RomaWasmStringRepresentation;
using google::scp::roma::wasm::WasmDeserializer;
using google::scp::roma::wasm::testing::WasmTestingUtils;
using google::scp::roma::worker::ExecutionUtils;
using ::testing::IsEmpty;
using ::testing::StrEq;

namespace google::scp::roma::worker::test {

class ExecutionUtilsTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    const int my_pid = getpid();
    const std::string proc_exe_path =
        std::string("/proc/") + std::to_string(my_pid) + "/exe";
    auto my_path = std::make_unique<char[]>(PATH_MAX);
    ssize_t sz = readlink(proc_exe_path.c_str(), my_path.get(), PATH_MAX);
    ASSERT_GT(sz, 0);
    v8::V8::InitializeICUDefaultLocation(my_path.get());
    v8::V8::InitializeExternalStartupData(my_path.get());
    platform_ = v8::platform::NewDefaultPlatform().release();
    v8::V8::InitializePlatform(platform_);
    v8::V8::Initialize();
  }

  void SetUp() override {
    create_params_.array_buffer_allocator =
        v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    isolate_ = v8::Isolate::New(create_params_);
  }

  void TearDown() override {
    isolate_->Dispose();
    delete create_params_.array_buffer_allocator;
  }

  struct RunCodeArguments {
    std::string js;
    std::string wasm;
    std::string handler_name;
    std::vector<std::string_view> input;
  };

  // RunCode() is used to create an executable environment to execute the code.
  ExecutionResult RunCode(const RunCodeArguments& args, std::string& output,
                          std::string& err_msg) noexcept {
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::TryCatch try_catch(isolate_);
    v8::Local<v8::Context> context = v8::Context::New(isolate_);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Value> handler;

    // Compile and run JavaScript code object when JavaScript code obj is
    // available.
    if (!args.js.empty()) {
      auto result = ExecutionUtils::CompileRunJS(args.js, err_msg);
      if (!result.Successful()) {
        std::cout << "Error CompileRunJS:" << err_msg << "\n" << std::endl;
        return result;
      }

      // Get handler value from compiled JS code.
      auto result_get_handler =
          ExecutionUtils::GetJsHandler(args.handler_name, handler, err_msg);
      if (!result_get_handler.Successful()) {
        return result_get_handler;
      }
    }

    bool is_wasm_run = false;

    // Compile and run wasm code object when only WASM is available.
    if (args.js.empty() && !args.wasm.empty()) {
      auto result = ExecutionUtils::CompileRunWASM(args.wasm, err_msg);
      if (!result.Successful()) {
        std::cout << "Error CompileRunWASM:" << err_msg << "\n" << std::endl;
        return result;
      }

      // Get handler value from compiled JS code.
      auto result_get_handler =
          ExecutionUtils::GetWasmHandler(args.handler_name, handler, err_msg);
      if (!result_get_handler.Successful()) {
        return result_get_handler;
      }

      is_wasm_run = true;
    }

    auto input = args.input;
    auto argc = input.size();
    v8::Local<v8::Value> argv[argc];
    {
      auto argv_array =
          ExecutionUtils::InputToLocalArgv(input, is_wasm_run).As<v8::Array>();
      // If argv_array size doesn't match with input. Input conversion failed.
      if (argv_array.IsEmpty() || argv_array->Length() != argc) {
        err_msg = ExecutionUtils::DescribeError(isolate_, &try_catch);
        return FailureExecutionResult(SC_ROMA_V8_WORKER_BAD_INPUT_ARGS);
      }
      for (size_t i = 0; i < argc; ++i) {
        argv[i] = argv_array->Get(context, i).ToLocalChecked();
      }
    }

    v8::Local<v8::Function> handler_func = handler.As<v8::Function>();
    v8::Local<v8::Value> result;
    if (!handler_func->Call(context, context->Global(), argc, argv)
             .ToLocal(&result)) {
      err_msg = ExecutionUtils::DescribeError(isolate_, &try_catch);
      return FailureExecutionResult(SC_ROMA_V8_WORKER_CODE_EXECUTION_FAILURE);
    }

    // If this is a WASM run then we need to deserialize the returned data
    if (is_wasm_run) {
      auto offset = result.As<v8::Int32>()->Value();

      result = ExecutionUtils::ReadFromWasmMemory(isolate_, context, offset);
    }

    auto json_string_maybe = v8::JSON::Stringify(context, result);
    v8::Local<v8::String> json_string;
    if (!json_string_maybe.ToLocal(&json_string)) {
      err_msg = ExecutionUtils::DescribeError(isolate_, &try_catch);
      return FailureExecutionResult(SC_ROMA_V8_WORKER_RESULT_PARSE_FAILURE);
    }

    v8::String::Utf8Value result_utf8(isolate_, json_string);
    output = std::string(*result_utf8, result_utf8.length());
    return SuccessExecutionResult();
  }

  v8::Isolate::CreateParams create_params_;
  static v8::Platform* platform_;
  v8::Isolate* isolate_{nullptr};
};

v8::Platform* ExecutionUtilsTest::platform_{nullptr};

TEST_F(ExecutionUtilsTest, InputToLocalArgv) {
  const auto input_list = std::vector<std::string_view>({"1", "2", "3"});
  {
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = v8::Context::New(isolate_);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Array> local_list =
        ExecutionUtils::InputToLocalArgv(input_list);
    for (size_t idx = 0; idx < input_list.size(); ++idx) {
      v8::String::Utf8Value output(
          isolate_,
          local_list->Get(context, idx).ToLocalChecked().As<v8::String>());
      auto expected = input_list.at(idx);
      EXPECT_EQ(*output, expected);
    }
  }
}

TEST_F(ExecutionUtilsTest, InputToLocalArgvJsonInput) {
  const auto list = std::vector<std::string_view>({
      R"({"value":1})",
      R"({"value":2})",
  });
  {
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = v8::Context::New(isolate_);
    v8::Context::Scope context_scope(context);

    std::vector<std::string_view> input(list.begin(), list.end());
    v8::Local<v8::Array> local_list = ExecutionUtils::InputToLocalArgv(input);

    for (size_t idx = 0; idx < list.size(); ++idx) {
      auto expected = std::string(list.at(idx));
      auto json_value = local_list->Get(context, idx).ToLocalChecked();
      v8::String::Utf8Value output(
          isolate_, v8::JSON::Stringify(context, json_value).ToLocalChecked());
      EXPECT_EQ(*output, expected);
    }
  }
}

TEST_F(ExecutionUtilsTest, InputToLocalArgvInvalidJsonInput) {
  const auto input_list = std::vector<std::string_view>({
      R"({favoriteFruit: "apple"})",
      R"({"value":2})",
  });
  {
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = v8::Context::New(isolate_);
    v8::Context::Scope context_scope(context);

    auto v8_array =
        ExecutionUtils::ExecutionUtils::InputToLocalArgv(input_list);

    EXPECT_TRUE(v8_array.IsEmpty());
  }
}

TEST_F(ExecutionUtilsTest, InputToLocalArgvByteStringInput) {
  const auto input_list = std::vector<std::string_view>(
      {"value_1", "value_2", "", R"({"json_value":123})"});
  {
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = v8::Context::New(isolate_);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Array> local_list = ExecutionUtils::InputToLocalArgv(
        input_list, /*is_wasm=*/false, /*is_byte_str=*/true);

    for (size_t idx = 0; idx < input_list.size(); ++idx) {
      auto expected = input_list.at(idx);
      auto str_value = local_list->Get(context, idx).ToLocalChecked();
      v8::String::Utf8Value output(isolate_, str_value);
      EXPECT_EQ(*output, expected);
    }
  }
}

TEST_F(ExecutionUtilsTest, InputToLocalArgvInputWithEmptyString) {
  const auto input_list =
      std::vector<std::string_view>({"", R"({"value":2})", "{}"});
  const auto expected_list =
      std::vector<std::string_view>({"undefined", R"({"value":2})", "{}"});
  {
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = v8::Context::New(isolate_);
    v8::Context::Scope context_scope(context);

    auto v8_array =
        ExecutionUtils::ExecutionUtils::InputToLocalArgv(input_list);
    for (size_t idx = 0; idx < v8_array->Length(); ++idx) {
      auto expected = expected_list.at(idx);
      auto json_value = v8_array->Get(context, idx).ToLocalChecked();
      v8::String::Utf8Value output(
          isolate_, v8::JSON::Stringify(context, json_value).ToLocalChecked());
      EXPECT_EQ(*output, expected);
    }
  }
}

TEST_F(ExecutionUtilsTest, RunCodeObjWithBadInput) {
  RunCodeArguments code_obj;
  code_obj.js =
      R"(
        function Handler(a, b) {
          return (a["value"] + b["value"]);
        }
      )";
  code_obj.handler_name = "Handler";
  code_obj.input =
      std::vector<std::string_view>({R"({value":1})", R"({"value":2})"});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_THAT(
      result,
      ResultIs(FailureExecutionResult(SC_ROMA_V8_WORKER_BAD_INPUT_ARGS)));
}

TEST_F(ExecutionUtilsTest, RunCodeObjWithJsonInput) {
  RunCodeArguments code_obj;
  code_obj.js =
      R"(
        function Handler(a, b) {
          return (a["value"] + b["value"]);
        }
      )";
  code_obj.handler_name = "Handler";
  code_obj.input = std::vector<std::string_view>({
      R"({"value":1})",
      R"({"value":2})",
  });

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
  EXPECT_THAT(output, StrEq("3"));
}

TEST_F(ExecutionUtilsTest, PerformanceNowDeclaredInJs) {
  RunCodeArguments code_obj;
  code_obj.js =
      R"(
        function Handler() {
          // Date.now overriden to always return the same number.
          Date.now = () => 1672531200000;
          return performance.now() === Date.now();
        }
      )";
  code_obj.handler_name = "Handler";

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
  EXPECT_THAT(output, StrEq("true"));
}

TEST_F(ExecutionUtilsTest, JsPredicateMatchesTrueOutput) {
  RunCodeArguments code_obj;
  code_obj.js = "var Predicate = () => true;";
  code_obj.handler_name = "Predicate";

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
  EXPECT_THAT(output, StrEq("true"));
}

TEST_F(ExecutionUtilsTest, JsPredicateMatchesFalseOutput) {
  RunCodeArguments code_obj;
  code_obj.js = "var Predicate = () => false;";
  code_obj.handler_name = "Predicate";

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
  EXPECT_THAT(output, StrEq("false"));
}

TEST_F(ExecutionUtilsTest, JsFunctionOutput) {
  RunCodeArguments code_obj;
  code_obj.js = "var Handler = () => 3;";
  code_obj.handler_name = "Handler";

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
  EXPECT_THAT(output, StrEq("3"));
}

TEST_F(ExecutionUtilsTest, RunCodeObjWithJsonInputMissKey) {
  RunCodeArguments code_obj;
  code_obj.js =
      R"(
        function Handler(a, b) {
          return (a.value + b.value);
        }
      )";
  code_obj.handler_name = "Handler";
  code_obj.input = std::vector<std::string_view>({
      "{:1}",
      R"({"value":2})",
  });

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_THAT(
      result,
      ResultIs(FailureExecutionResult(SC_ROMA_V8_WORKER_BAD_INPUT_ARGS)));
}

TEST_F(ExecutionUtilsTest, RunCodeObjWithJsonInputMissValue) {
  RunCodeArguments code_obj;
  code_obj.js =
      R"(
        function Handler(a, b) {
          return (a["value"] + b.value);
        }
      )";
  code_obj.handler_name = "Handler";
  code_obj.input = std::vector<std::string_view>({
      R"({"value"})",
      R"({"value":2})",
  });

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_THAT(
      result,
      ResultIs(FailureExecutionResult(SC_ROMA_V8_WORKER_BAD_INPUT_ARGS)));
}

TEST_F(ExecutionUtilsTest, RunCodeObjRunWithLessArgs) {
  // When handler function argument is Json data, function still can call and
  // run without any error, but there is no valid output.
  RunCodeArguments code_obj;
  code_obj.js = "var Handler = (a, b) => (a + b);";
  code_obj.handler_name = "Handler";

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
  EXPECT_THAT(output, StrEq("null"));
}

TEST_F(ExecutionUtilsTest, RunCodeObjRunWithJsonArgsMissing) {
  // When handler function argument is Json data, empty input will cause
  // function call fail.
  RunCodeArguments code_obj;
  code_obj.js =
      R"(
        function Handler(a, b) {
          return (a.value + b.value);
        }
      )";
  code_obj.handler_name = "Handler";

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_EQ(result,
            FailureExecutionResult(SC_ROMA_V8_WORKER_CODE_EXECUTION_FAILURE));
}

TEST_F(ExecutionUtilsTest, NoHandlerName) {
  RunCodeArguments code_obj;
  code_obj.js = "function Handler(a, b) {return;}";
  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_THAT(
      result,
      ResultIs(FailureExecutionResult(SC_ROMA_V8_WORKER_BAD_HANDLER_NAME)));
}

TEST_F(ExecutionUtilsTest, UnmatchedHandlerName) {
  RunCodeArguments code_obj;
  code_obj.js = "function Handler(a, b) {return;}";
  code_obj.handler_name = "Handler2";

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_EQ(result,
            FailureExecutionResult(SC_ROMA_V8_WORKER_HANDLER_INVALID_FUNCTION));
}

TEST_F(ExecutionUtilsTest, ScriptCompileFailure) {
  std::string output;
  std::string err_msg;
  std::string js("function Handler(a, b) {");
  {
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = v8::Context::New(isolate_);
    v8::Context::Scope context_scope(context);
    auto result = ExecutionUtils::CompileRunJS(js, err_msg);
    EXPECT_EQ(result,
              FailureExecutionResult(SC_ROMA_V8_WORKER_CODE_COMPILE_FAILURE));
  }
}

TEST_F(ExecutionUtilsTest, SuccessWithUnNeedArgs) {
  RunCodeArguments code_obj;
  code_obj.handler_name = "Handler";
  code_obj.js = "function Handler() {return;}";
  code_obj.input = std::vector<std::string_view>({"1", "0"});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
}

TEST_F(ExecutionUtilsTest, CodeExecutionFailure) {
  RunCodeArguments code_obj;
  code_obj.handler_name = "Handler";
  code_obj.js = "function Handler() { throw new Error('Required'); return;}";

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_EQ(result,
            FailureExecutionResult(SC_ROMA_V8_WORKER_CODE_EXECUTION_FAILURE));
}

TEST_F(ExecutionUtilsTest, WasmSourceCodeCompileFailed) {
  RunCodeArguments code_obj;
  code_obj.js = "";
  code_obj.handler_name = "add";
  // Bad wasm byte string.
  char wasm_bin[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
                     0x01, 0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01};
  code_obj.wasm.assign(wasm_bin, sizeof(wasm_bin));
  code_obj.input = std::vector<std::string_view>({"1", "2"});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_EQ(result,
            FailureExecutionResult(SC_ROMA_V8_WORKER_WASM_COMPILE_FAILURE));
}

TEST_F(ExecutionUtilsTest, WasmSourceCodeUnmatchedName) {
  RunCodeArguments code_obj;
  code_obj.js = "";
  code_obj.handler_name = "plus";
  char wasm_bin[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01,
                     0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03,
                     0x02, 0x01, 0x00, 0x07, 0x07, 0x01, 0x03, 0x61, 0x64,
                     0x64, 0x00, 0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20,
                     0x00, 0x20, 0x01, 0x6a, 0x0b};
  code_obj.wasm.assign(wasm_bin, sizeof(wasm_bin));
  code_obj.input = std::vector<std::string_view>({"1", "2"});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_EQ(result,
            FailureExecutionResult(SC_ROMA_V8_WORKER_HANDLER_INVALID_FUNCTION));
  EXPECT_THAT(output, IsEmpty());
}

TEST_F(ExecutionUtilsTest, CppWasmWithStringInputAndStringOutput) {
  RunCodeArguments code_obj;
  code_obj.js = "";
  code_obj.handler_name = "Handler";

  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./scp/cc/roma/testing/cpp_wasm_string_in_string_out_example/"
      "string_in_string_out.wasm");
  code_obj.wasm.assign(reinterpret_cast<char*>(wasm_bin.data()),
                       wasm_bin.size());
  code_obj.input = std::vector<std::string_view>({
      R"str("Input String :)")str",
  });

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
  EXPECT_THAT(output, StrEq(R"("Input String :) Hello World from WASM")"));
}

TEST_F(ExecutionUtilsTest, RustWasmWithStringInputAndStringOutput) {
  RunCodeArguments code_obj;
  code_obj.js = "";
  code_obj.handler_name = "Handler";

  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./scp/cc/roma/testing/rust_wasm_string_in_string_out_example/"
      "string_in_string_out.wasm");
  code_obj.wasm.assign(reinterpret_cast<char*>(wasm_bin.data()),
                       wasm_bin.size());
  code_obj.input = std::vector<std::string_view>({
      R"str("Input String :)")str",
  });

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
  EXPECT_THAT(output, StrEq(R"("Input String :) Hello from rust!")"));
}

TEST_F(ExecutionUtilsTest, JsEmbeddedGlobalWasmCompileRunExecute) {
  RunCodeArguments code_obj;
  std::string js = R"(
          let bytes = new Uint8Array([
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
            0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
            0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
            0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b
          ]);
          let module = new WebAssembly.Module(bytes);
          let instance = new WebAssembly.Instance(module);
          function Handler(a, b) {
          return instance.exports.add(a, b);
          }
        )";
  code_obj.js = js;
  code_obj.handler_name = "Handler";
  code_obj.input = std::vector<std::string_view>({"1", "2"});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
  EXPECT_THAT(output, StrEq(std::to_string(3).c_str()));
}
}  // namespace google::scp::roma::worker::test
