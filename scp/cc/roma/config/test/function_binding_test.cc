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

// These tests validate the C++/JS function binding. That is, the registration
// of a C++ function which gets called when a JS code block invokes it.

#include <gtest/gtest.h>

#include <linux/limits.h>

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <libplatform/libplatform.h>

#include "absl/functional/bind_front.h"
#include "include/v8.h"
#include "roma/config/src/function_binding_object.h"
#include "roma/config/src/type_converter.h"

namespace google::scp::roma::config::test {
class FunctionBindingTest : public ::testing::Test {
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

  static v8::Platform* platform_;
  v8::Isolate::CreateParams create_params_;
  v8::Isolate* isolate_;
};

v8::Platform* FunctionBindingTest::platform_{nullptr};

// Entry point to be able to call the user-provided JS function
static void GlobalV8FunctionCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  auto isolate = info.GetIsolate();
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);

  // Get the user-provided function
  v8::Local<v8::External> data_object =
      v8::Local<v8::External>::Cast(info.Data());
  auto user_function =
      reinterpret_cast<FunctionBindingObjectBase*>(data_object->Value());

  user_function->InvokeInternalHandler(info);
}

static std::string RunV8Function(v8::Isolate* isolate, std::string source_js,
                                 FunctionBindingObjectBase& function_binding) {
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::ObjectTemplate> global_object_template =
      v8::ObjectTemplate::New(isolate);

  global_object_template->SetInternalFieldCount(1);

  auto function_name = TypeConverter<std::string>::ToV8(
                           isolate, function_binding.GetFunctionName())
                           .As<v8::String>();

  // Allow retrieving the user-provided function from the
  // v8::FunctionCallbackInfo when the C++ callback is invoked so that it can be
  // called.
  v8::Local<v8::External> user_provided_function =
      v8::External::New(isolate, reinterpret_cast<void*>(&function_binding));
  auto function_template = v8::FunctionTemplate::New(
      isolate, &GlobalV8FunctionCallback, user_provided_function);

  // set the global function
  global_object_template->Set(function_name, function_template);
  // Set the global object template on the context
  v8::Local<v8::Context> global_context =
      v8::Context::New(isolate, nullptr, global_object_template);

  v8::Context::Scope context_scope(global_context);

  // Execute the JS code source. Which should call the function that we
  // registered by name from JS code
  v8::Local<v8::String> source =
      v8::String::NewFromUtf8(isolate, source_js.c_str(),
                              v8::NewStringType::kNormal)
          .ToLocalChecked();
  v8::Local<v8::Script> script =
      v8::Script::Compile(global_context, source).ToLocalChecked();

  v8::TryCatch try_catch(isolate);
  v8::MaybeLocal<v8::Value> result = script->Run(global_context);

  // See if execution generated any errors
  if (try_catch.HasCaught()) {
    std::string error_message;
    if (try_catch.Message().IsEmpty() ||
        !TypeConverter<std::string>::FromV8(isolate, try_catch.Message()->Get(),
                                            &error_message)) {
      error_message = "FAILED_EXECUTION";
    }
    return error_message;
  }

  v8::String::Utf8Value result_as_string(isolate, result.ToLocalChecked());
  auto result_str = std::string(*result_as_string);

  return result_str;
}

// User provided JS function
static std::string StringInputStringOutput(std::tuple<std::string>& input) {
  return std::get<0>(input) + " " +
         "Value added within user-provided function call";
}

TEST_F(FunctionBindingTest, FunctionBindingByNameStringInputAndStringOutput) {
  // Function that returns a string and takes in a string as input
  FunctionBindingObject<std::string, std::string> func;
  // Set the name by which the function will be called in JS
  func.function_name = "str_in_str_out";
  // Set the C++ callback for the JS function
  func.function = StringInputStringOutput;

  auto result =
      RunV8Function(isolate_, "str_in_str_out('Hello from JS!');", func);

  EXPECT_EQ(result,
            "Hello from JS! Value added within user-provided function call");
}

TEST_F(FunctionBindingTest,
       FunctionBindingByNameStringInputAndStringOutputInvalidTypeInputInt) {
  // Function that returns a string and takes in a string as input
  FunctionBindingObject<std::string, std::string> func;
  // Set the name by which the function will be called in JS
  func.function_name = "str_in_str_out";
  // Set the C++ callback for the JS function
  func.function = StringInputStringOutput;

  auto result = RunV8Function(isolate_, "str_in_str_out(1);", func);

  EXPECT_EQ(result,
            "Uncaught Error: (str_in_str_out) Error encountered while "
            "converting types");
}

TEST_F(
    FunctionBindingTest,
    FunctionBindingByNameStringInputAndStringOutputInvalidTypeInputListOfInt) {
  // Function that returns a string and takes in a string as input
  FunctionBindingObject<std::string, std::string> func;
  // Set the name by which the function will be called in JS
  func.function_name = "str_in_str_out";
  // Set the C++ callback for the JS function
  func.function = StringInputStringOutput;

  auto result = RunV8Function(isolate_, "str_in_str_out([1,2,3]);", func);

  EXPECT_EQ(result,
            "Uncaught Error: (str_in_str_out) Error encountered while "
            "converting types");
}

TEST_F(FunctionBindingTest,
       StringInputAndStringOutputInvalidTypeInputListOfString) {
  // Function that returns a string and takes in a string as input
  FunctionBindingObject<std::string, std::string> func;
  // Set the name by which the function will be called in JS
  func.function_name = "str_in_str_out";
  // Set the C++ callback for the JS function
  func.function = StringInputStringOutput;

  auto result = RunV8Function(isolate_, "str_in_str_out(['Hel', 'lo']);", func);

  EXPECT_EQ(result,
            "Uncaught Error: (str_in_str_out) Error encountered while "
            "converting types");
}

TEST_F(FunctionBindingTest,
       FunctionBindingByNameStringInputAndStringOutputInvalidTypeInputObject) {
  // Function that returns a string and takes in a string as input
  FunctionBindingObject<std::string, std::string> func;
  // Set the name by which the function will be called in JS
  func.function_name = "str_in_str_out";
  // Set the C++ callback for the JS function
  func.function = StringInputStringOutput;

  auto result = RunV8Function(isolate_, "obj = {}; str_in_str_out(obj);", func);

  EXPECT_EQ(result,
            "Uncaught Error: (str_in_str_out) Error encountered while "
            "converting types");
}

TEST_F(FunctionBindingTest, PassingLessArgumentsThanExpected) {
  // Function that returns a string and takes in a string as input
  FunctionBindingObject<std::string, std::string> func;
  // Set the name by which the function will be called in JS
  func.function_name = "str_in_str_out";
  // Set the C++ callback for the JS function
  func.function = StringInputStringOutput;

  auto result = RunV8Function(isolate_, "str_in_str_out();", func);

  EXPECT_EQ(result,
            "Uncaught Error: (str_in_str_out) Unexpected number of inputs");
}

TEST_F(FunctionBindingTest, PassingMoreArgumentsThanExpected) {
  // Function that returns a string and takes in a string as input
  FunctionBindingObject<std::string, std::string> func;
  // Set the name by which the function will be called in JS
  func.function_name = "str_in_str_out";
  // Set the C++ callback for the JS function
  func.function = StringInputStringOutput;

  auto result = RunV8Function(
      isolate_, "str_in_str_out('All good', 'Unexpected');", func);

  EXPECT_EQ(result,
            "Uncaught Error: (str_in_str_out) Unexpected number of inputs");
}

TEST_F(FunctionBindingTest, PassingUndefinedValueToFunction) {
  // Function that returns a string and takes in a string as input
  FunctionBindingObject<std::string, std::string> func;
  // Set the name by which the function will be called in JS
  func.function_name = "str_in_str_out";
  // Set the C++ callback for the JS function
  func.function = StringInputStringOutput;

  auto result =
      RunV8Function(isolate_, "a = undefined; str_in_str_out(a);", func);

  EXPECT_EQ(result,
            "Uncaught Error: (str_in_str_out) Error encountered while "
            "converting types");
}

TEST_F(FunctionBindingTest, PassingNullValueToFunction) {
  // Function that returns a string and takes in a string as input
  FunctionBindingObject<std::string, std::string> func;
  // Set the name by which the function will be called in JS
  func.function_name = "str_in_str_out";
  // Set the C++ callback for the JS function
  func.function = StringInputStringOutput;

  auto result = RunV8Function(isolate_, "a = null; str_in_str_out(a);", func);

  EXPECT_EQ(result,
            "Uncaught Error: (str_in_str_out) Error encountered while "
            "converting types");
}

// User provided JS function
static std::vector<std::string> StringInputVectorOfStringOutput(
    std::tuple<std::string>& input) {
  std::vector<std::string> output;
  output.push_back(std::get<0>(input));
  output.push_back("And some added stuff");
  return output;
}

// User provided JS function
static std::vector<std::string> VectorOfStringInputVectorOfStringOutput(
    std::tuple<std::vector<std::string>>& input) {
  auto input_vec = std::get<0>(input);

  std::vector<std::string> output;
  // Reverse the input
  for (int i = input_vec.size() - 1; i >= 0; i--) {
    output.push_back(input_vec.at(i));
  }
  return output;
}

static std::string ConcatenateVector(std::vector<std::string>& vec) {
  std::string ret;
  for (const auto& str : vec) {
    ret += str;
  }
  return ret;
}

// User provided JS function
static std::vector<std::string> MixedInputAndVectorOfStringOutput(
    std::tuple<std::vector<std::string>, std::string, std::vector<std::string>,
               std::string>& input) {
  auto input_one = std::get<0>(input);
  auto input_two = std::get<1>(input);
  auto input_three = std::get<2>(input);
  auto input_four = std::get<3>(input);

  std::vector<std::string> output;
  output.push_back(ConcatenateVector(input_one));
  output.push_back(input_two);
  output.push_back(ConcatenateVector(input_three));
  output.push_back(input_four);
  return output;
}

TEST_F(FunctionBindingTest, ShouldAllowInlineHandler) {
  // Function that returns a string and takes a string as input
  FunctionBindingObject<std::string, std::string> func;
  // Set the name by which the function will be called in JS
  func.function_name = "func_that_calls_lambda";
  // Set the C++ callback for the JS function
  func.function = [](std::tuple<std::string>& input) -> std::string {
    return std::get<0>(input) + "-From lambda";
  };

  auto js_source =
      "result = func_that_calls_lambda('From JS');"
      "result;";

  auto result = RunV8Function(isolate_, js_source, func);

  EXPECT_EQ(result, "From JS-From lambda");
}

class MyHandler {
 public:
  std::string HookHandler(std::tuple<std::string>& input) {
    return std::get<0>(input) + "-From member function";
  }
};

TEST_F(FunctionBindingTest, ShouldAllowMemberFunctionAsHandler) {
  // Instance whose member function we want to call
  MyHandler my_handler;

  // Function that returns a string and takes a string as input
  FunctionBindingObject<std::string, std::string> func;
  // Set the name by which the function will be called in JS
  func.function_name = "func_that_calls_member_func";
  // Set the C++ callback for the JS function
  func.function = absl::bind_front(&MyHandler::HookHandler, my_handler);

  auto js_source =
      "result = func_that_calls_member_func('From JS');"
      "result;";

  auto result = RunV8Function(isolate_, js_source, func);

  EXPECT_EQ(result, "From JS-From member function");
}

}  // namespace google::scp::roma::config::test
