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

#include "src/roma/worker/execution_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "include/libplatform/libplatform.h"
#include "include/v8-context.h"
#include "include/v8-initialization.h"
#include "include/v8-isolate.h"
#include "src/roma/wasm/testing_utils.h"
#include "src/util/process_util.h"
#include "src/util/status_macro/status_macros.h"

using google::scp::roma::wasm::testing::WasmTestingUtils;
using google::scp::roma::worker::ExecutionUtils;
using ::testing::IsEmpty;
using ::testing::StrEq;

namespace google::scp::roma::worker::test {

class ExecutionUtilsTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    absl::StatusOr<std::string> my_path =
        ::privacy_sandbox::server_common::GetExePath();
    CHECK_OK(my_path);
    v8::V8::InitializeICUDefaultLocation(my_path->data());
    v8::V8::InitializeExternalStartupData(my_path->data());
    platform_ = v8::platform::NewDefaultPlatform().release();
    v8::V8::InitializePlatform(platform_);
    v8::V8::Initialize();
  }

  static void TearDownTestSuite() {
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
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
    std::string handler_name;
    std::string js;
    std::string wasm;
    std::vector<std::string_view> input;
  };

  // RunCode() is used to create an executable environment to execute the code.
  absl::Status RunCode(const RunCodeArguments& args, std::string& output) {
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::TryCatch try_catch(isolate_);
    v8::Local<v8::Context> context = v8::Context::New(isolate_);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Value> handler;

    // Compile and run JavaScript code object when JavaScript code obj is
    // available.
    if (!args.js.empty()) {
      PS_RETURN_IF_ERROR(ExecutionUtils::CompileRunJS(args.js));
      PS_RETURN_IF_ERROR(
          ExecutionUtils::GetJsHandler(args.handler_name, handler));
    }

    bool is_wasm_run = false;

    // Compile and run wasm code object when only WASM is available.
    if (args.js.empty() && !args.wasm.empty()) {
      auto result = ExecutionUtils::CompileRunWASM(args.wasm);
      if (!result.ok()) {
        std::cout << "Error CompileRunWASM:" << result.message() << std::endl;
        return result;
      }

      // Get handler value from compiled JS code.
      auto result_get_handler =
          ExecutionUtils::GetWasmHandler(args.handler_name, handler);
      if (!result_get_handler.ok()) {
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
        privacy_sandbox::server_common::StatusBuilder builder(
            absl::InvalidArgumentError("Bad input arguments"));
        builder << ExecutionUtils::DescribeError(isolate_, &try_catch);
        return builder;
      }
      for (size_t i = 0; i < argc; ++i) {
        argv[i] = argv_array->Get(context, i).ToLocalChecked();
      }
    }

    v8::Local<v8::Function> handler_func = handler.As<v8::Function>();
    v8::Local<v8::Value> result;
    if (!handler_func->Call(context, context->Global(), argc, argv)
             .ToLocal(&result)) {
      privacy_sandbox::server_common::StatusBuilder builder(
          absl::InvalidArgumentError("Failed to run JavaScript code object"));
      builder << ExecutionUtils::DescribeError(isolate_, &try_catch);
      return builder;
    }

    // If this is a WASM run then we need to deserialize the returned data
    if (is_wasm_run) {
      auto offset = result.As<v8::Int32>()->Value();

      result = ExecutionUtils::ReadFromWasmMemory(isolate_, context, offset);
    }

    auto json_string_maybe = v8::JSON::Stringify(context, result);
    v8::Local<v8::String> json_string;
    if (!json_string_maybe.ToLocal(&json_string)) {
      privacy_sandbox::server_common::StatusBuilder builder(
          absl::InvalidArgumentError("Unable to parse JSON"));
      builder << ExecutionUtils::DescribeError(isolate_, &try_catch);
      return builder;
    }

    v8::String::Utf8Value result_utf8(isolate_, json_string);
    output = std::string(*result_utf8, result_utf8.length());
    return absl::OkStatus();
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
      v8::MaybeLocal<v8::Value> maybe = local_list->Get(context, idx);
      ASSERT_FALSE(maybe.IsEmpty());
      const v8::Local<v8::Value>& value = maybe.ToLocalChecked();
      ASSERT_TRUE(value->IsNumber());
      const v8::String::Utf8Value output(isolate_, value.As<v8::Number>());
      EXPECT_EQ(*output, input_list.at(idx));
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
  constexpr std::string_view bad_arg1 = R"({value":1})";
  constexpr std::string_view arg2 = R"({"value":2})";
  RunCodeArguments code_obj{
      .handler_name = "Handler",
      .js =
          R"(
        function Handler(a, b) {
          return (a["value"] + b["value"]);
        }
      )",
      .input = std::vector<std::string_view>({bad_arg1, arg2}),
  };

  std::string output;
  EXPECT_EQ(absl::StatusCode::kInvalidArgument,
            RunCode(code_obj, output).code())
      << " Output: " << output;
}

TEST_F(ExecutionUtilsTest, RunCodeObjWithJsonInput) {
  const RunCodeArguments code_obj = {
      .handler_name = "Handler",
      .js =
          R"(
        function Handler(a, b) {
          return (a["value"] + b["intval"]);
        }
      )",
      .input = {R"({"value":1})", R"({"intval":2})"},
  };

  std::string output;
  ASSERT_TRUE(RunCode(code_obj, output).ok());
  EXPECT_THAT(output, StrEq("3"));
}

TEST_F(ExecutionUtilsTest, RunCodeObjWithNamespacedHandler) {
  const RunCodeArguments code_obj = {
      .handler_name = "foo.bar.Handler",
      .js =
          R"(
        var foo = {
          bar: {
            Handler: (a, b) => { return a["value"] + b["intval"]; },
          },
        };
      )",
      .input = {R"({"value":1})", R"({"intval":2})"},
  };
  std::string output;
  ASSERT_TRUE(RunCode(code_obj, output).ok());
  EXPECT_THAT(output, StrEq("3"));
}

TEST_F(ExecutionUtilsTest, JsPredicateMatchesTrueOutput) {
  const RunCodeArguments code_obj = {
      .handler_name = "Predicate",
      .js = "var Predicate = () => true;",
  };
  std::string output;
  ASSERT_TRUE(RunCode(code_obj, output).ok());
  EXPECT_THAT(output, StrEq("true"));
}

TEST_F(ExecutionUtilsTest, JsPredicateMatchesFalseOutput) {
  const RunCodeArguments code_obj = {
      .handler_name = "Predicate",
      .js = "var Predicate = () => false;",
  };
  std::string output;
  ASSERT_TRUE(RunCode(code_obj, output).ok());
  EXPECT_THAT(output, StrEq("false"));
}

TEST_F(ExecutionUtilsTest, JsFunctionOutput) {
  const RunCodeArguments code_obj = {
      .handler_name = "Handler",
      .js = "var Handler = () => 3;",
  };
  std::string output;
  ASSERT_TRUE(RunCode(code_obj, output).ok());
  EXPECT_THAT(output, StrEq("3"));
}

TEST_F(ExecutionUtilsTest, RunCodeObjWithJsonInputMissKey) {
  const RunCodeArguments code_obj = {
      .handler_name = "Handler",
      .js =
          R"(
        function Handler(a, b) {
          return (a.value + b.value);
        }
      )",
      .input = {"{:1}", R"({"value":2})"},
  };
  std::string output;
  EXPECT_EQ(absl::StatusCode::kInvalidArgument,
            RunCode(code_obj, output).code())
      << " Output: " << output;
}

TEST_F(ExecutionUtilsTest, RunCodeObjWithJsonInputMissValue) {
  const RunCodeArguments code_obj = {
      .handler_name = "Handler",
      .js =
          R"(
        function Handler(a, b) {
          return (a["value"] + b.value);
        }
      )",
      .input = {R"({"value"})", R"({"value":2})"},
  };
  std::string output;
  EXPECT_EQ(absl::StatusCode::kInvalidArgument,
            RunCode(code_obj, output).code())
      << " Output: " << output;
}

TEST_F(ExecutionUtilsTest, RunCodeObjRunWithLessArgs) {
  // When handler function argument is Json data, function still can call and
  // run without any error, but there is no valid output.
  const RunCodeArguments code_obj = {
      .handler_name = "Handler",
      .js = "var Handler = (a, b) => (a + b);",
  };
  std::string output;
  ASSERT_TRUE(RunCode(code_obj, output).ok());
  EXPECT_THAT(output, StrEq("null"));
}

TEST_F(ExecutionUtilsTest, RunCodeObjRunWithJsonArgsMissing) {
  // When handler function argument is Json data, empty input will cause
  // function call fail.
  const RunCodeArguments code_obj = {
      .handler_name = "Handler",
      .js =
          R"(
        function Handler(a, b) {
          return (a.value + b.value);
        }
      )",
  };
  std::string output;
  EXPECT_EQ(absl::StatusCode::kInvalidArgument,
            RunCode(code_obj, output).code())
      << " Output: " << output;
}

TEST_F(ExecutionUtilsTest, NoHandlerName) {
  const RunCodeArguments code_obj = {
      .js = "function Handler(a, b) {return;}",
  };
  std::string output;
  EXPECT_EQ(absl::StatusCode::kInvalidArgument,
            RunCode(code_obj, output).code())
      << " Output: " << output;
}

TEST_F(ExecutionUtilsTest, UnmatchedHandlerName) {
  const RunCodeArguments code_obj = {
      .handler_name = "Handler2",
      .js = "function Handler(a, b) {return;}",
  };
  std::string output;
  EXPECT_EQ(absl::StatusCode::kNotFound, RunCode(code_obj, output).code())
      << " Output: " << output;
}

TEST_F(ExecutionUtilsTest, ScriptCompileFailure) {
  constexpr std::string_view bad_js = "function Handler(a, b) {";
  {
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = v8::Context::New(isolate_);
    v8::Context::Scope context_scope(context);
    EXPECT_EQ(absl::StatusCode::kInvalidArgument,
              ExecutionUtils::CompileRunJS(bad_js).code());
  }
}

TEST_F(ExecutionUtilsTest, SuccessWithUnNeedArgs) {
  const RunCodeArguments code_obj = {
      .handler_name = "Handler",
      .js = "function Handler() {return;}",
      .input = {"1", "0"},
  };
  std::string output;
  EXPECT_TRUE(RunCode(code_obj, output).ok());
}

TEST_F(ExecutionUtilsTest, CodeExecutionFailure) {
  const RunCodeArguments code_obj = {
      .handler_name = "Handler",
      .js = "function Handler() { throw new Error('Required'); return;}",
  };
  std::string output;
  EXPECT_EQ(absl::StatusCode::kInvalidArgument,
            RunCode(code_obj, output).code())
      << " Output: " << output;
}

TEST_F(ExecutionUtilsTest, WasmSourceCodeCompileFailed) {
  // Bad wasm byte string.
  const char bad_wasm_bin[] = {
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
      0x01, 0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01,
  };
  const RunCodeArguments code_obj = {
      .handler_name = "add",
      .js = "",
      .wasm = std::string(bad_wasm_bin, sizeof(bad_wasm_bin)),
      .input = {"1", "2"},
  };
  std::string output;
  EXPECT_EQ(absl::StatusCode::kInvalidArgument,
            RunCode(code_obj, output).code())
      << " Output: " << output;
}

TEST_F(ExecutionUtilsTest, WasmSourceCodeUnmatchedName) {
  const char wasm_bin[] = {
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
      0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
      0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
      0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b,
  };
  const RunCodeArguments code_obj = {
      .handler_name = "plus",
      .js = "",
      .wasm = std::string(wasm_bin, sizeof(wasm_bin)),
      .input = {"1", "2"},
  };

  std::string output;
  EXPECT_EQ(absl::StatusCode::kInvalidArgument,
            RunCode(code_obj, output).code())
      << " Output: " << output;
  EXPECT_THAT(output, IsEmpty());
}

TEST_F(ExecutionUtilsTest, CppWasmWithStringInputAndStringOutput) {
  const auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./src/roma/testing/cpp_wasm_string_in_string_out_example/"
      "string_in_string_out.wasm");
  const RunCodeArguments code_obj = {
      .handler_name = "Handler",
      .js = "",
      .wasm = std::string(reinterpret_cast<const char*>(wasm_bin.data()),
                          wasm_bin.size()),
      .input = {R"str("Input String :)")str"},
  };
  std::string output;
  ASSERT_TRUE(RunCode(code_obj, output).ok());
  EXPECT_THAT(output, StrEq(R"("Input String :) Hello World from WASM")"));
}

TEST_F(ExecutionUtilsTest, RustWasmWithStringInputAndStringOutput) {
  const auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./src/roma/testing/rust_wasm_string_in_string_out_example/"
      "string_in_string_out.wasm");
  const RunCodeArguments code_obj = {
      .handler_name = "Handler",
      .js = "",
      .wasm = std::string(reinterpret_cast<const char*>(wasm_bin.data()),
                          wasm_bin.size()),
      .input = {R"str("Input String :)")str"},
  };
  std::string output;
  ASSERT_TRUE(RunCode(code_obj, output).ok());
  EXPECT_THAT(output, StrEq(R"("Input String :) Hello from rust!")"));
}

TEST_F(ExecutionUtilsTest, JsEmbeddedGlobalWasmCompileRunExecute) {
  const RunCodeArguments code_obj = {
      .handler_name = "Handler",
      .js = R"(
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
        )",
      .input = {"1", "2"},
  };

  std::string output;
  ASSERT_TRUE(RunCode(code_obj, output).ok());
  EXPECT_THAT(output, StrEq(std::to_string(3).c_str()));
}

}  // namespace google::scp::roma::worker::test
