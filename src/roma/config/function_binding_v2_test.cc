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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "include/libplatform/libplatform.h"
#include "include/v8.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/config/type_converter.h"
#include "src/util/process_util.h"

using ::testing::StrEq;

namespace google::scp::roma::config::test {
namespace {
bool V8TypesToProto(const v8::FunctionCallbackInfo<v8::Value>& info,
                    proto::FunctionBindingIoProto& proto) {
  if (info.Length() == 0) {
    // No arguments were passed to function
    return true;
  }

  auto isolate = info.GetIsolate();
  const auto& function_parameter = info[0];

  using StrVec = std::vector<std::string>;
  using StrMap = absl::flat_hash_map<std::string, std::string>;

  // Try to convert to one of the supported types
  if (std::string string_native; TypeConverter<std::string>::FromV8(
          isolate, function_parameter, &string_native)) {
    proto.set_input_string(std::move(string_native));
  } else if (StrVec vector_of_string_native; TypeConverter<StrVec>::FromV8(
                 isolate, function_parameter, &vector_of_string_native)) {
    proto.mutable_input_list_of_string()->mutable_data()->Reserve(
        vector_of_string_native.size());
    for (auto&& native : vector_of_string_native) {
      proto.mutable_input_list_of_string()->mutable_data()->Add(
          std::move(native));
    }
  } else if (StrMap map_of_string_native; TypeConverter<StrMap>::FromV8(
                 isolate, function_parameter, &map_of_string_native)) {
    for (auto&& [key, value] : map_of_string_native) {
      (*proto.mutable_input_map_of_string()->mutable_data())[std::move(key)] =
          std::move(value);
    }
  } else if (function_parameter->IsUint8Array()) {
    const auto array = function_parameter.As<v8::Uint8Array>();
    const auto data_len = array->Length();
    std::string native_data;
    native_data.resize(data_len);
    if (!TypeConverter<uint8_t*>::FromV8(isolate, function_parameter,
                                         native_data)) {
      return false;
    }
    *proto.mutable_input_bytes() = std::move(native_data);
  } else {
    // Unknown type
    return false;
  }

  return true;
}

v8::Local<v8::Value> ProtoToV8Type(v8::Isolate* isolate,
                                   const proto::FunctionBindingIoProto& proto) {
  if (proto.has_output_string()) {
    return TypeConverter<std::string>::ToV8(isolate, proto.output_string());
  } else if (proto.has_output_list_of_string()) {
    return TypeConverter<std::vector<std::string>>::ToV8(
        isolate, proto.output_list_of_string().data());
  } else if (proto.has_output_map_of_string()) {
    return TypeConverter<absl::flat_hash_map<std::string, std::string>>::ToV8(
        isolate, proto.output_map_of_string().data());
  } else if (proto.has_output_bytes()) {
    return TypeConverter<uint8_t*>::ToV8(isolate, proto.output_bytes());
  }

  return v8::Undefined(isolate);
}
}  // namespace

class FunctionBindingTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    absl::StatusOr<std::string> my_path =
        ::privacy_sandbox::server_common::GetExePath();
    CHECK_OK(my_path) << my_path.status();
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
      reinterpret_cast<FunctionBindingObjectV2<>*>(data_object->Value());

  if (info.Length() != 1) {
    isolate->ThrowError(
        TypeConverter<std::string>::ToV8(
            isolate, absl::StrCat("(", user_function->function_name,
                                  ") Unexpected number of inputs"))
            .As<v8::String>());
    return;
  }

  proto::FunctionBindingIoProto io_proto;

  if (!V8TypesToProto(info, io_proto)) {
    isolate->ThrowError(
        TypeConverter<std::string>::ToV8(
            isolate, absl::StrCat("(", user_function->function_name,
                                  ") Error encountered while converting types"))
            .As<v8::String>());
    return;
  }

  FunctionBindingPayload<> wrapper{io_proto, {}};

  user_function->function(wrapper);
  const auto& returned_value = ProtoToV8Type(isolate, io_proto);
  info.GetReturnValue().Set(returned_value);
}

static std::string RunV8Function(v8::Isolate* isolate, std::string source_js,
                                 FunctionBindingObjectV2<>& function_binding) {
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::ObjectTemplate> global_object_template =
      v8::ObjectTemplate::New(isolate);

  global_object_template->SetInternalFieldCount(1);

  auto function_name =
      TypeConverter<std::string>::ToV8(isolate, function_binding.function_name)
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
void StringInputStringOutput(FunctionBindingPayload<>& payload) {
  payload.io_proto.set_output_string(
      absl::StrCat(payload.io_proto.input_string(), " ",
                   "Value added within user-provided function call"));
}

TEST_F(FunctionBindingTest, FunctionBindingByNameStringInputAndStringOutput) {
  // Function that returns a string and takes in a string as input
  FunctionBindingObjectV2 obj;
  // Set the name by which the function will be called in JS
  obj.function_name = "str_in_str_out";
  // Set the C++ callback for the JS function
  obj.function = StringInputStringOutput;

  auto result =
      RunV8Function(isolate_, "str_in_str_out('Hello from JS!');", obj);

  EXPECT_THAT(
      result,
      StrEq("Hello from JS! Value added within user-provided function call"));
}

TEST_F(FunctionBindingTest,
       FunctionBindingByNameStringInputAndStringOutputInvalidTypeInputInt) {
  // Function that returns a string and takes in a string as input
  FunctionBindingObjectV2 obj;
  // Set the name by which the function will be called in JS
  obj.function_name = "str_in_str_out";
  // Set the C++ callback for the JS function
  obj.function = StringInputStringOutput;

  auto result = RunV8Function(isolate_, "str_in_str_out(1);", obj);

  EXPECT_THAT(result, StrEq("Uncaught Error: (str_in_str_out) Error "
                            "encountered while converting types"));
}

TEST_F(
    FunctionBindingTest,
    FunctionBindingByNameStringInputAndStringOutputInvalidTypeInputListOfInt) {
  // Function that returns a string and takes in a string as input
  FunctionBindingObjectV2 obj;
  // Set the name by which the function will be called in JS
  obj.function_name = "str_in_str_out";
  // Set the C++ callback for the JS function
  obj.function = StringInputStringOutput;

  auto result = RunV8Function(isolate_, "str_in_str_out([1,2,3]);", obj);

  EXPECT_THAT(result,
              StrEq("Uncaught Error: (str_in_str_out) Error encountered while "
                    "converting types"));
}

TEST_F(FunctionBindingTest,
       FunctionBindingByNameStringInputAndStringOutputInvalidTypeInputObject) {
  // Function that returns a string and takes in a string as input
  FunctionBindingObjectV2 obj;
  // Set the name by which the function will be called in JS
  obj.function_name = "str_in_str_out";
  // Set the C++ callback for the JS function
  obj.function = StringInputStringOutput;

  auto result = RunV8Function(isolate_, "obj = {}; str_in_str_out(obj);", obj);

  EXPECT_THAT(result,
              StrEq("Uncaught Error: (str_in_str_out) Error encountered while "
                    "converting types"));
}

TEST_F(FunctionBindingTest, PassingLessArgumentsThanExpected) {
  // Function that returns a string and takes in a string as input
  FunctionBindingObjectV2 obj;
  // Set the name by which the function will be called in JS
  obj.function_name = "str_in_str_out";
  // Set the C++ callback for the JS function
  obj.function = StringInputStringOutput;

  auto result = RunV8Function(isolate_, "str_in_str_out();", obj);

  EXPECT_THAT(
      result,
      StrEq("Uncaught Error: (str_in_str_out) Unexpected number of inputs"));
}

TEST_F(FunctionBindingTest, PassingMoreArgumentsThanExpected) {
  // Function that returns a string and takes in a string as input
  FunctionBindingObjectV2 obj;
  // Set the name by which the function will be called in JS
  obj.function_name = "str_in_str_out";
  // Set the C++ callback for the JS function
  obj.function = StringInputStringOutput;

  auto result =
      RunV8Function(isolate_, "str_in_str_out('All good', 'Unexpected');", obj);

  EXPECT_THAT(
      result,
      StrEq("Uncaught Error: (str_in_str_out) Unexpected number of inputs"));
}

TEST_F(FunctionBindingTest, PassingUndefinedValueToFunction) {
  // Function that returns a string and takes in a string as input
  FunctionBindingObjectV2 obj;
  // Set the name by which the function will be called in JS
  obj.function_name = "str_in_str_out";
  // Set the C++ callback for the JS function
  obj.function = StringInputStringOutput;

  auto result =
      RunV8Function(isolate_, "a = undefined; str_in_str_out(a);", obj);

  EXPECT_THAT(result,
              StrEq("Uncaught Error: (str_in_str_out) Error encountered while "
                    "converting types"));
}

TEST_F(FunctionBindingTest, PassingNullValueToFunction) {
  // Function that returns a string and takes in a string as input
  FunctionBindingObjectV2 obj;
  // Set the name by which the function will be called in JS
  obj.function_name = "str_in_str_out";
  // Set the C++ callback for the JS function
  obj.function = StringInputStringOutput;

  auto result = RunV8Function(isolate_, "a = null; str_in_str_out(a);", obj);

  EXPECT_THAT(result,
              StrEq("Uncaught Error: (str_in_str_out) Error encountered while "
                    "converting types"));
}

TEST_F(FunctionBindingTest, ShouldAllowInlineHandler) {
  // Function that returns a string and takes a string as input
  FunctionBindingObjectV2 obj;
  // Set the name by which the function will be called in JS
  obj.function_name = "func_that_calls_lambda";
  // Set the C++ callback for the JS function
  obj.function = [](FunctionBindingPayload<>& payload) -> void {
    payload.io_proto.set_output_string(
        absl::StrCat(payload.io_proto.input_string(), "-From lambda"));
  };

  auto js_source =
      "result = func_that_calls_lambda('From JS');"
      "result;";

  auto result = RunV8Function(isolate_, js_source, obj);

  EXPECT_THAT(result, StrEq("From JS-From lambda"));
}

class MyHandler {
 public:
  void HookHandler(FunctionBindingPayload<>& payload) {
    payload.io_proto.set_output_string(
        absl::StrCat(payload.io_proto.input_string(), "-From member function"));
  }
};

TEST_F(FunctionBindingTest, ShouldAllowMemberFunctionAsHandler) {
  // Instance whose member function we want to call
  MyHandler my_handler;

  // Function that returns a string and takes a string as input
  FunctionBindingObjectV2 obj;
  // Set the name by which the function will be called in JS
  obj.function_name = "func_that_calls_member_func";
  // Set the C++ callback for the JS function
  obj.function = absl::bind_front(&MyHandler::HookHandler, my_handler);

  auto js_source =
      "result = func_that_calls_member_func('From JS');"
      "result;";

  auto result = RunV8Function(isolate_, js_source, obj);

  EXPECT_THAT(result, StrEq("From JS-From member function"));
}

}  // namespace google::scp::roma::config::test
