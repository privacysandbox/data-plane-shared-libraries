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
#include "roma/common/src/containers.h"
#include "roma/config/src/function_binding_object.h"
#include "roma/config/src/type_converter.h"

using std::runtime_error;
using std::tuple;
using v8::Context;
using v8::External;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::ObjectTemplate;
using v8::Script;
using v8::String;
using v8::TryCatch;
using v8::Value;

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
    isolate_ = Isolate::New(create_params_);
  }

  void TearDown() override {
    isolate_->Dispose();
    delete create_params_.array_buffer_allocator;
  }

  static v8::Platform* platform_;
  Isolate::CreateParams create_params_;
  Isolate* isolate_;
};

v8::Platform* FunctionBindingTest::platform_{nullptr};

// Entry point to be able to call the user-provided JS function
static void GlobalV8FunctionCallback(const FunctionCallbackInfo<Value>& info) {
  auto isolate = info.GetIsolate();
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);

  // Get the user-provided function
  Local<External> data_object = Local<External>::Cast(info.Data());
  auto user_function =
      reinterpret_cast<FunctionBindingObjectBase*>(data_object->Value());

  user_function->InvokeInternalHandler(info);
}

static std::string RunV8Function(Isolate* isolate, std::string source_js,
                                 FunctionBindingObjectBase& function_binding) {
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);

  Local<ObjectTemplate> global_object_template = ObjectTemplate::New(isolate);

  global_object_template->SetInternalFieldCount(1);

  auto function_name = TypeConverter<std::string>::ToV8(
                           isolate, function_binding.GetFunctionName())
                           .As<String>();

  // Allow retrieving the user-provided function from the FunctionCallbackInfo
  // when the C++ callback is invoked so that it can be called.
  Local<External> user_provided_function =
      External::New(isolate, reinterpret_cast<void*>(&function_binding));
  auto function_template = v8::FunctionTemplate::New(
      isolate, &GlobalV8FunctionCallback, user_provided_function);

  // set the global function
  global_object_template->Set(function_name, function_template);
  // Set the global object template on the context
  Local<Context> global_context =
      Context::New(isolate, nullptr, global_object_template);

  Context::Scope context_scope(global_context);

  // Execute the JS code source. Which should call the function that we
  // registered by name from JS code
  Local<String> source = String::NewFromUtf8(isolate, source_js.c_str(),
                                             v8::NewStringType::kNormal)
                             .ToLocalChecked();
  Local<Script> script =
      Script::Compile(global_context, source).ToLocalChecked();

  TryCatch try_catch(isolate);
  MaybeLocal<Value> result = script->Run(global_context);

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

  String::Utf8Value result_as_string(isolate, result.ToLocalChecked());
  auto result_str = std::string(*result_as_string);

  return result_str;
}

// User provided JS function
static std::string StringInputStringOutput(tuple<std::string>& input) {
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
    tuple<std::string>& input) {
  std::vector<std::string> output;
  output.push_back(std::get<0>(input));
  output.push_back("And some added stuff");
  return output;
}

TEST_F(FunctionBindingTest,
       FunctionBindingByNameStringInputAndVectorOfStringOutput) {
  // Function that returns a vector of string and takes in a string as input
  FunctionBindingObject<std::vector<std::string>, std::string> func;
  // Set the name by which the function will be called in JS
  func.function_name = "str_in_vec_str_out";
  // Set the C++ callback for the JS function
  func.function = StringInputVectorOfStringOutput;

  auto result =
      RunV8Function(isolate_, "str_in_vec_str_out('Hello from JS!');", func);

  EXPECT_EQ(result, "Hello from JS!,And some added stuff");
}

// User provided JS function
static std::vector<std::string> VectorOfStringInputVectorOfStringOutput(
    tuple<std::vector<std::string>>& input) {
  auto input_vec = std::get<0>(input);

  std::vector<std::string> output;
  // Reverse the input
  for (int i = input_vec.size() - 1; i >= 0; i--) {
    output.push_back(input_vec.at(i));
  }
  return output;
}

TEST_F(FunctionBindingTest,
       FunctionBindingByNameVectorOfStringInputAndVectorOfStringOutput) {
  // Function that returns a vector of string string and takes in a vector of
  // string as input
  FunctionBindingObject<std::vector<std::string>, std::vector<std::string>>
      func;
  // Set the name by which the function will be called in JS
  func.function_name = "vec_str_in_vec_str_out";
  // Set the C++ callback for the JS function
  func.function = VectorOfStringInputVectorOfStringOutput;

  auto result = RunV8Function(
      isolate_, "vec_str_in_vec_str_out(['H','E','L','L','O']);", func);

  EXPECT_EQ(result, "O,L,L,E,H");
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
    tuple<std::vector<std::string>, std::string, std::vector<std::string>,
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

TEST_F(FunctionBindingTest, VectorOfStringOutputAndMixedInput) {
  // Function that returns a vector of string and takes mixed types as input
  FunctionBindingObject<std::vector<std::string>, std::vector<std::string>,
                        std::string, std::vector<std::string>, std::string>
      func;
  // Set the name by which the function will be called in JS
  func.function_name = "mixed_in_vec_str_out";
  // Set the C++ callback for the JS function
  func.function = MixedInputAndVectorOfStringOutput;

  auto js_source =
      "list_one = ['H','E','L','L','O'];"
      "str_one = 'MY';"
      "list_two = ['F','R','I','E','N','D'];"
      "str_two = ':)';"
      "mixed_in_vec_str_out(list_one, str_one, list_two, str_two)";

  auto result = RunV8Function(isolate_, js_source, func);

  EXPECT_EQ(result, "HELLO,MY,FRIEND,:)");
}

// User provided JS function
static common::Map<std::string, std::string> VectorsOfStringInputAndMapOutput(
    tuple<std::vector<std::string>, std::vector<std::string>,
          std::vector<std::string>, std::vector<std::string>>& input) {
  auto input_one = std::get<0>(input);
  auto input_two = std::get<1>(input);
  auto input_three = std::get<2>(input);
  auto input_four = std::get<3>(input);

  common::Map<std::string, std::string> output;
  output.Set("vec1", ConcatenateVector(input_one));
  output.Set("vec2", ConcatenateVector(input_two));
  output.Set("vec3", ConcatenateVector(input_three));
  output.Set("vec4", ConcatenateVector(input_four));
  return output;
}

TEST_F(FunctionBindingTest, MapOutputAndVectorsOfStringInput) {
  // Function that returns a map<string, string >and takes vectors of string as
  // input
  FunctionBindingObject<common::Map<std::string, std::string>,
                        std::vector<std::string>, std::vector<std::string>,
                        std::vector<std::string>, std::vector<std::string>>
      func;
  // Set the name by which the function will be called in JS
  func.function_name = "vecs_str_in_map_out";
  // Set the C++ callback for the JS function
  func.function = VectorsOfStringInputAndMapOutput;

  auto js_source =
      "list_one = ['A','B','C','D','E'];"
      "list_two = ['F','G','H','I','J'];"
      "list_three = ['K','L','M','N','O'];"
      "list_four = ['P','Q','R','S','T'];"
      "map = vecs_str_in_map_out(list_one, list_two, list_three, list_four);"
      "result = [];"
      "for (let [key, value] of  map.entries()) {"
      "entry = key + '-' + value;"
      "result.push(entry);"
      "}"
      "result;";

  auto result = RunV8Function(isolate_, js_source, func);

  EXPECT_EQ(result, "vec1-ABCDE,vec2-FGHIJ,vec3-KLMNO,vec4-PQRST");
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
