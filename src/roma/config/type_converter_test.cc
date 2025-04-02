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

#include "src/roma/config/type_converter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "include/libplatform/libplatform.h"
#include "include/v8.h"
#include "src/roma/interface/function_binding_io.pb.h"
#include "src/util/process_util.h"

using ::testing::ElementsAreArray;
using ::testing::StrEq;

namespace google::scp::roma::config::test {
class TypeConverterTest : public ::testing::Test {
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

v8::Platform* TypeConverterTest::platform_{nullptr};

static void ExpectStringEquality(v8::Isolate* isolate,
                                 std::string_view native_str,
                                 const v8::Local<v8::String>& v8_str) {
  EXPECT_EQ(v8_str->Length(), native_str.size());
  EXPECT_THAT(*v8::String::Utf8Value(isolate, v8_str), StrEq(native_str));
}

TEST_F(TypeConverterTest, NativeStringToV8) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);

  std::string native_str = "I am a string";

  v8::Local<v8::String> v8_str =
      TypeConverter<std::string>::ToV8(isolate_, native_str).As<v8::String>();

  ExpectStringEquality(isolate_, native_str, v8_str);
}

TEST_F(TypeConverterTest, v8StringToNative) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);

  v8::Local<v8::String> v8_str =
      v8::String::NewFromUtf8(isolate_, "I am a string").ToLocalChecked();

  std::string native_str;
  EXPECT_TRUE(
      TypeConverter<std::string>::FromV8(isolate_, v8_str, &native_str));

  ExpectStringEquality(isolate_, native_str, v8_str);
}

TEST_F(TypeConverterTest, v8StringToNativeFailsWhenNotString) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);

  v8::Local<v8::Number> v8_number = v8::Number::New(isolate_, 1);

  std::string native_str;
  EXPECT_FALSE(
      TypeConverter<std::string>::FromV8(isolate_, v8_number, &native_str));

  EXPECT_TRUE(native_str.empty());
}

static void ExpectArrayEquality(v8::Isolate* isolate,
                                const std::vector<std::string>& vec,
                                const v8::Local<v8::Array>& v8_array) {
  EXPECT_EQ(v8_array->Length(), vec.size());
  for (uint32_t i = 0; i < v8_array->Length(); i++) {
    auto v8_array_item = v8_array->Get(isolate->GetCurrentContext(), i)
                             .ToLocalChecked()
                             .As<v8::String>();
    ExpectStringEquality(isolate, vec.at(i), v8_array_item);
  }
}

void TestStringListConversionToV8ArrayTest(
    v8::Isolate* isolate, const std::vector<std::string>& vec) {
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  // Array allocation requires a context
  v8::Local<v8::Context> global_context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(global_context);

  proto::FunctionBindingIoProto io_proto;
  io_proto.mutable_output_list_of_string()->mutable_data()->Add(vec.begin(),
                                                                vec.end());
  const v8::Local<v8::Array> v8_array =
      TypeConverter<std::vector<std::string>>::ToV8(
          isolate, io_proto.output_list_of_string().data())
          .As<v8::Array>();
  ExpectArrayEquality(isolate, vec, v8_array);
}

TEST_F(TypeConverterTest, EmptyListOfStringProtoToV8Array) {
  TestStringListConversionToV8ArrayTest(isolate_, {});
}

TEST_F(TypeConverterTest, ListOfStringProtoToV8Array) {
  TestStringListConversionToV8ArrayTest(isolate_, {"one", "two", "three"});
}

void PopulateV8Array(v8::Local<v8::Context> global_context,
                     v8::Isolate* isolate, v8::Local<v8::Array> v8_array,
                     const std::vector<std::string>& elems) {
  for (int i = 0; i < elems.size(); ++i) {
    const std::string& elem = elems[i];
    v8_array
        ->Set(global_context, i,
              v8::String::NewFromUtf8(isolate, elem.data(),
                                      v8::NewStringType::kNormal, elem.size())
                  .ToLocalChecked())
        .Check();
  }
}

TEST_F(TypeConverterTest, EmptyV8ArrayToVectorOfString) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  // Array allocation requires a context
  v8::Local<v8::Context> global_context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(global_context);

  v8::Local<v8::Array> v8_array = v8::Array::New(isolate_, 0);
  PopulateV8Array(global_context, isolate_, v8_array, {});
  std::vector<std::string> vec;
  EXPECT_TRUE(TypeConverter<std::vector<std::string>>::FromV8(isolate_,
                                                              v8_array, &vec));
  ExpectArrayEquality(isolate_, vec, v8_array);
}

TEST_F(TypeConverterTest, v8ArrayEmptyStrElemToVectorOfString) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  // Array allocation requires a context
  v8::Local<v8::Context> global_context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(global_context);

  v8::Local<v8::Array> v8_array = v8::Array::New(isolate_, 1);
  PopulateV8Array(global_context, isolate_, v8_array, {""});
  std::vector<std::string> vec;
  EXPECT_TRUE(TypeConverter<std::vector<std::string>>::FromV8(isolate_,
                                                              v8_array, &vec));
  ExpectArrayEquality(isolate_, vec, v8_array);
}

TEST_F(TypeConverterTest, v8ArrayToVectorOfString) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  // Array allocation requires a context
  v8::Local<v8::Context> global_context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(global_context);

  v8::Local<v8::Array> v8_array = v8::Array::New(isolate_, 3);
  PopulateV8Array(global_context, isolate_, v8_array, {"one", "two", "three"});
  std::vector<std::string> vec;
  EXPECT_TRUE(TypeConverter<std::vector<std::string>>::FromV8(isolate_,
                                                              v8_array, &vec));
  ExpectArrayEquality(isolate_, vec, v8_array);
}

TEST_F(TypeConverterTest, v8IntegerArrayToVectorOfStringFails) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  // Array allocation requires a context
  v8::Local<v8::Context> global_context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(global_context);

  v8::Local<v8::Array> v8_array = v8::Array::New(isolate_, 1);
  v8_array->Set(global_context, 0, v8::Number::New(isolate_, 1)).Check();

  std::vector<std::string> vec;
  EXPECT_FALSE(TypeConverter<std::vector<std::string>>::FromV8(isolate_,
                                                               v8_array, &vec));
  EXPECT_TRUE(vec.empty());
}

TEST_F(TypeConverterTest, v8MixedArrayToVectorOfStringFails) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  // Array allocation requires a context
  v8::Local<v8::Context> global_context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(global_context);

  v8::Local<v8::Array> v8_array = v8::Array::New(isolate_, 3);
  v8_array
      ->Set(global_context, 0, v8::String::NewFromUtf8Literal(isolate_, "str1"))
      .Check();
  // Array has string, but also a number in there
  v8_array->Set(global_context, 1, v8::Number::New(isolate_, 1)).Check();
  v8_array
      ->Set(global_context, 2, v8::String::NewFromUtf8Literal(isolate_, "str2"))
      .Check();

  std::vector<std::string> vec;
  EXPECT_FALSE(TypeConverter<std::vector<std::string>>::FromV8(isolate_,
                                                               v8_array, &vec));

  EXPECT_THAT(vec, testing::IsEmpty());
}

static void AssertFlatHashMapOfStringEquality(
    v8::Isolate* isolate,
    const absl::flat_hash_map<std::string, std::string>& map,
    const v8::Local<v8::Map>& v8_map) {
  EXPECT_EQ(v8_map->Size(), map.size());

  // This turns the map into an array of size Size()*2, where index N is a
  // key, and N+1 is the value for the given key.
  auto v8_map_as_array = v8_map->AsArray();
  std::vector<std::string> native_map_keys;
  std::vector<std::string> native_map_vals;
  for (auto& kvp : map) {
    native_map_keys.push_back(kvp.first);
    native_map_vals.push_back(kvp.second);
  }

  std::vector<std::string> v8_map_keys;
  std::vector<std::string> v8_map_vals;
  for (size_t i = 0; i < v8_map_as_array->Length(); i += 2) {
    const auto key_index = i;
    const auto value_index = i + 1;

    auto v8_key = v8_map_as_array->Get(isolate->GetCurrentContext(), key_index)
                      .ToLocalChecked()
                      .As<v8::String>();
    auto v8_val =
        v8_map_as_array->Get(isolate->GetCurrentContext(), value_index)
            .ToLocalChecked()
            .As<v8::String>();

    std::string v8_map_key_converted;
    TypeConverter<std::string>::FromV8(isolate, v8_key, &v8_map_key_converted);
    v8_map_keys.push_back(v8_map_key_converted);

    std::string v8_map_val_converted;
    TypeConverter<std::string>::FromV8(isolate, v8_val, &v8_map_val_converted);
    v8_map_vals.push_back(v8_map_val_converted);
  }

  std::sort(native_map_keys.begin(), native_map_keys.end());
  std::sort(native_map_vals.begin(), native_map_vals.end());
  std::sort(v8_map_keys.begin(), v8_map_keys.end());
  std::sort(v8_map_vals.begin(), v8_map_vals.end());

  EXPECT_THAT(native_map_keys, ElementsAreArray(v8_map_keys));
  EXPECT_THAT(native_map_vals, ElementsAreArray(v8_map_vals));
}

TEST_F(TypeConverterTest, MapOfStringStringProtoToV8Map) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  // Array allocation requires a context
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  const absl::flat_hash_map<std::string, std::string> kv_map = {
      {"key1", "val1"},
      {"key2", "val2"},
      {"key3", "val3"},
  };

  proto::FunctionBindingIoProto io_proto;
  (*io_proto.mutable_output_map_of_string()->mutable_data())
      .insert(kv_map.begin(), kv_map.end());

  const v8::Local<v8::Map> v8_map =
      TypeConverter<absl::flat_hash_map<std::string, std::string>>::ToV8(
          isolate_, io_proto.output_map_of_string().data())
          .As<v8::Map>();

  AssertFlatHashMapOfStringEquality(isolate_, kv_map, v8_map);
}

TEST_F(TypeConverterTest, v8MapToFlatHashMapOfStringString) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  // Array allocation requires a context
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Map> v8_map = v8::Map::New(isolate_);
  v8_map
      ->Set(context, v8::String::NewFromUtf8Literal(isolate_, "key1"),
            v8::String::NewFromUtf8Literal(isolate_, "val1"))
      .ToLocalChecked();
  v8_map
      ->Set(context, v8::String::NewFromUtf8Literal(isolate_, "key2"),
            v8::String::NewFromUtf8Literal(isolate_, "val2"))
      .ToLocalChecked();
  v8_map
      ->Set(context, v8::String::NewFromUtf8Literal(isolate_, "key3"),
            v8::String::NewFromUtf8Literal(isolate_, "val3"))
      .ToLocalChecked();

  absl::flat_hash_map<std::string, std::string> map;
  EXPECT_TRUE(
      (TypeConverter<absl::flat_hash_map<std::string, std::string>>::FromV8(
          isolate_, v8_map, &map)));

  AssertFlatHashMapOfStringEquality(isolate_, map, v8_map);
}

TEST_F(TypeConverterTest,
       v8MapToFlatHashMapOfStringStringShouldFailWithUnsupportedTypeKey) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  // Array allocation requires a context
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Map> v8_map = v8::Map::New(isolate_);
  v8_map
      ->Set(context, v8::String::NewFromUtf8Literal(isolate_, "key1"),
            v8::String::NewFromUtf8Literal(isolate_, "val1"))
      .ToLocalChecked();
  // Number key
  v8_map
      ->Set(context, v8::Number::New(isolate_, 1),
            v8::String::NewFromUtf8Literal(isolate_, "val2"))
      .ToLocalChecked();
  v8_map
      ->Set(context, v8::String::NewFromUtf8Literal(isolate_, "key3"),
            v8::String::NewFromUtf8Literal(isolate_, "val3"))
      .ToLocalChecked();
  // Number value
  v8_map
      ->Set(context, v8::String::NewFromUtf8Literal(isolate_, "key4"),
            v8::Number::New(isolate_, 1))
      .ToLocalChecked();

  absl::flat_hash_map<std::string, std::string> map;
  EXPECT_FALSE(
      (TypeConverter<absl::flat_hash_map<std::string, std::string>>::FromV8(
          isolate_, v8_map, &map)));
  EXPECT_THAT(map, testing::IsEmpty());
}

TEST_F(TypeConverterTest,
       v8MapToFlatHashMapOfStringStringShouldFailWithUnsupportedMixedTypes) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  // Array allocation requires a context
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Map> v8_map = v8::Map::New(isolate_);
  v8_map
      ->Set(context, v8::Number::New(isolate_, 1),
            v8::String::NewFromUtf8Literal(isolate_, "val1"))
      .ToLocalChecked();

  absl::flat_hash_map<std::string, std::string> map;
  EXPECT_FALSE(
      (TypeConverter<absl::flat_hash_map<std::string, std::string>>::FromV8(
          isolate_, v8_map, &map)));
  EXPECT_THAT(map, testing::IsEmpty());
}

TEST_F(TypeConverterTest, NativeUint32ToV8) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);

  constexpr uint32_t native_val = 1234;
  v8::Local<v8::Uint32> v8_val =
      TypeConverter<uint32_t>::ToV8(isolate_, native_val).As<v8::Uint32>();

  EXPECT_EQ(v8_val->Value(), native_val);
}

TEST_F(TypeConverterTest, v8Uint32ToNative) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);

  auto v8_val = v8::Integer::NewFromUnsigned(isolate_, 4567);

  uint32_t native_val;
  EXPECT_TRUE(TypeConverter<uint32_t>::FromV8(isolate_, v8_val, &native_val));
  EXPECT_EQ(native_val, 4567);
}

TEST_F(TypeConverterTest, v8Uint32ToNativeShouldFailWithUnknownType) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);

  auto v8_val = TypeConverter<std::string>::ToV8(isolate_, "a string");

  uint32_t native_val;
  EXPECT_FALSE(TypeConverter<uint32_t>::FromV8(isolate_, v8_val, &native_val));
}

TEST_F(TypeConverterTest, NativeUint8PointerToV8) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  // Array allocation requires a context
  v8::Local<v8::Context> global_context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(global_context);

  const std::vector<uint8_t> native_val = {1, 2, 3, 4};

  v8::Local<v8::Uint8Array> v8_val =
      TypeConverter<uint8_t*>::ToV8(isolate_, native_val.data(),
                                    native_val.size())
          .As<v8::Uint8Array>();

  // Make sure sizes match
  EXPECT_EQ(v8_val->Length(), native_val.size());

  // Compare the actual values
  for (int i = 0; i < native_val.size(); i++) {
    const auto val = v8_val->Get(global_context, i).ToLocalChecked();
    uint32_t native_int;
    TypeConverter<uint32_t>::FromV8(isolate_, val, &native_int);
    EXPECT_EQ(native_int, native_val.at(i));
  }

  // The above should be enough, but also compare the buffers to be thorough
  EXPECT_EQ(
      memcmp(native_val.data(), v8_val->Buffer()->Data(), v8_val->Length()), 0);
}

TEST_F(TypeConverterTest, V8Uint8ArrayToNativeUint8Pointer) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  // Array allocation requires a context
  v8::Local<v8::Context> global_context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(global_context);

  // Create a v8::Uint8Array
  auto data_size = 3;
  auto buffer = v8::ArrayBuffer::New(isolate_, data_size);
  auto v8_val = v8::Uint8Array::New(buffer, 0, data_size);
  v8_val->Set(global_context, 0, v8::Integer::New(isolate_, 3)).Check();
  v8_val->Set(global_context, 1, v8::Integer::New(isolate_, 2)).Check();
  v8_val->Set(global_context, 2, v8::Integer::New(isolate_, 1)).Check();

  auto out_data = std::make_unique<uint8_t[]>(data_size);
  EXPECT_TRUE(TypeConverter<uint8_t*>::FromV8(isolate_, v8_val, out_data.get(),
                                              data_size));

  // Compare the actual values
  for (int i = 0; i < v8_val->Length(); i++) {
    const auto val = v8_val->Get(global_context, i).ToLocalChecked();
    uint32_t native_int;
    TypeConverter<uint32_t>::FromV8(isolate_, val, &native_int);
    EXPECT_EQ(native_int, out_data.get()[i]);
  }

  // The above should be enough, but also compare the buffers to be thorough
  EXPECT_EQ(memcmp(out_data.get(), v8_val->Buffer()->Data(), v8_val->Length()),
            0);
}

nlohmann::json ProtoValueToJson(const google::protobuf::Value& value) {
  switch (value.kind_case()) {
    case google::protobuf::Value::kNullValue:
      return nullptr;
    case google::protobuf::Value::kNumberValue:
      return value.number_value();
    case google::protobuf::Value::kStringValue:
      return value.string_value();
    case google::protobuf::Value::kBoolValue:
      return value.bool_value();
    case google::protobuf::Value::kStructValue: {
      nlohmann::json obj = nlohmann::json::object();
      for (const auto& [key, nested_value] : value.struct_value().fields()) {
        obj[key] = ProtoValueToJson(nested_value);
      }
      return obj;
    }
    case google::protobuf::Value::kListValue: {
      nlohmann::json arr = nlohmann::json::array();
      for (const auto& item : value.list_value().values()) {
        arr.push_back(ProtoValueToJson(item));
      }
      return arr;
    }
    default:
      return nullptr;
  }
}

TEST_F(TypeConverterTest, V8ObjectToProtobufStruct) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  const std::string json_str = R"({
    "string_key": "string_value",
    "number_key": 42.5,
    "bool_key": true,
    "null_key": null,
    "array_key": ["item1", 2, false],
    "object_key": {
      "nested_key": "nested_value"
    }
  })";

  v8::Local<v8::String> v8_json =
      v8::String::NewFromUtf8(isolate_, json_str.c_str()).ToLocalChecked();
  v8::Local<v8::Value> v8_obj_value;
  bool parsed = v8::JSON::Parse(context, v8_json).ToLocal(&v8_obj_value);
  ASSERT_TRUE(parsed) << "Failed to parse JSON";
  ASSERT_TRUE(v8_obj_value->IsObject()) << "Parsed value is not an object";

  v8::Local<v8::Object> v8_obj = v8_obj_value.As<v8::Object>();

  google::protobuf::Struct proto_struct;
  EXPECT_TRUE(TypeConverter<google::protobuf::Struct>::FromV8(isolate_, v8_obj,
                                                              &proto_struct));

  nlohmann::json original_json = nlohmann::json::parse(json_str);

  nlohmann::json result_json = nlohmann::json::object();
  for (const auto& [key, value] : proto_struct.fields()) {
    result_json[key] = ProtoValueToJson(value);
  }

  EXPECT_EQ(original_json, result_json) << "Original: " << original_json.dump(2)
                                        << "\nResult: " << result_json.dump(2);
}

TEST_F(TypeConverterTest, V8ObjectToProtobufStructInvalidInput) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  std::string input = "not_an_object";
  v8::Local<v8::Value> v8_val =
      v8::String::NewFromUtf8(isolate_, input.c_str()).ToLocalChecked();

  google::protobuf::Struct proto_struct;
  EXPECT_FALSE(TypeConverter<google::protobuf::Struct>::FromV8(isolate_, v8_val,
                                                               &proto_struct));

  EXPECT_EQ(proto_struct.fields_size(), 0)
      << "Struct should be empty after failed conversion";
}

TEST_F(TypeConverterTest, V8ObjectToProtobufStructEmptyObject) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  const std::string json_str = "{}";

  v8::Local<v8::String> v8_json =
      v8::String::NewFromUtf8(isolate_, json_str.c_str()).ToLocalChecked();
  v8::Local<v8::Value> v8_obj_value;
  bool parsed = v8::JSON::Parse(context, v8_json).ToLocal(&v8_obj_value);
  ASSERT_TRUE(parsed) << "Failed to parse JSON";
  ASSERT_TRUE(v8_obj_value->IsObject()) << "Parsed value is not an object";

  v8::Local<v8::Object> v8_obj = v8_obj_value.As<v8::Object>();

  google::protobuf::Struct proto_struct;
  EXPECT_TRUE(TypeConverter<google::protobuf::Struct>::FromV8(isolate_, v8_obj,
                                                              &proto_struct));

  EXPECT_EQ(proto_struct.fields_size(), 0)
      << "Empty object should result in empty fields";
}

TEST_F(TypeConverterTest, V8ObjectToProtobufStructDeeplyNested) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  const std::string json_str = R"({
    "level1": {
      "level2": {
        "level3": {
          "level4": {
            "level5": "deep value"
          }
        }
      }
    }
  })";

  v8::Local<v8::String> v8_json =
      v8::String::NewFromUtf8(isolate_, json_str.c_str()).ToLocalChecked();
  v8::Local<v8::Value> v8_obj_value;
  bool parsed = v8::JSON::Parse(context, v8_json).ToLocal(&v8_obj_value);
  ASSERT_TRUE(parsed) << "Failed to parse JSON";
  ASSERT_TRUE(v8_obj_value->IsObject()) << "Parsed value is not an object";

  v8::Local<v8::Object> v8_obj = v8_obj_value.As<v8::Object>();

  google::protobuf::Struct proto_struct;
  EXPECT_TRUE(TypeConverter<google::protobuf::Struct>::FromV8(isolate_, v8_obj,
                                                              &proto_struct));

  nlohmann::json original_json = nlohmann::json::parse(json_str);
  nlohmann::json result_json = nlohmann::json::object();
  for (const auto& [key, value] : proto_struct.fields()) {
    result_json[key] = ProtoValueToJson(value);
  }

  EXPECT_EQ(original_json, result_json);
}

TEST_F(TypeConverterTest, V8ObjectToProtobufStructComplexArrays) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  const std::string json_str = R"({
    "mixed_array": [1, "string", true, null, {"key": "value"}, [1, 2, 3]],
    "empty_array": [],
    "array_of_arrays": [[], [1, 2], ["a", "b"]]
  })";

  v8::Local<v8::String> v8_json =
      v8::String::NewFromUtf8(isolate_, json_str.c_str()).ToLocalChecked();
  v8::Local<v8::Value> v8_obj_value;
  bool parsed = v8::JSON::Parse(context, v8_json).ToLocal(&v8_obj_value);
  ASSERT_TRUE(parsed) << "Failed to parse JSON";
  ASSERT_TRUE(v8_obj_value->IsObject()) << "Parsed value is not an object";

  v8::Local<v8::Object> v8_obj = v8_obj_value.As<v8::Object>();

  google::protobuf::Struct proto_struct;
  EXPECT_TRUE(TypeConverter<google::protobuf::Struct>::FromV8(isolate_, v8_obj,
                                                              &proto_struct));

  nlohmann::json original_json = nlohmann::json::parse(json_str);
  nlohmann::json result_json = nlohmann::json::object();
  for (const auto& [key, value] : proto_struct.fields()) {
    result_json[key] = ProtoValueToJson(value);
  }

  EXPECT_EQ(original_json, result_json);
}

TEST_F(TypeConverterTest, V8ObjectToProtobufStructSpecialValues) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> v8_obj = v8::Object::New(isolate_);
  v8_obj
      ->Set(context,
            v8::String::NewFromUtf8(isolate_, "pos_infinity").ToLocalChecked(),
            v8::Number::New(isolate_, std::numeric_limits<double>::infinity()))
      .Check();
  v8_obj
      ->Set(context,
            v8::String::NewFromUtf8(isolate_, "neg_infinity").ToLocalChecked(),
            v8::Number::New(isolate_, -std::numeric_limits<double>::infinity()))
      .Check();
  v8_obj
      ->Set(context, v8::String::NewFromUtf8(isolate_, "nan").ToLocalChecked(),
            v8::Number::New(isolate_, std::numeric_limits<double>::quiet_NaN()))
      .Check();
  v8_obj
      ->Set(context,
            v8::String::NewFromUtf8(isolate_, "max_number").ToLocalChecked(),
            v8::Number::New(isolate_, std::numeric_limits<double>::max()))
      .Check();
  v8_obj
      ->Set(context,
            v8::String::NewFromUtf8(isolate_, "min_number").ToLocalChecked(),
            v8::Number::New(isolate_, std::numeric_limits<double>::min()))
      .Check();
  v8_obj
      ->Set(context,
            v8::String::NewFromUtf8(isolate_, "empty_string").ToLocalChecked(),
            v8::String::NewFromUtf8(isolate_, "").ToLocalChecked())
      .Check();

  google::protobuf::Struct proto_struct;
  EXPECT_TRUE(TypeConverter<google::protobuf::Struct>::FromV8(isolate_, v8_obj,
                                                              &proto_struct));

  EXPECT_EQ(proto_struct.fields_size(), 6) << "Should have 6 fields";
  EXPECT_TRUE(proto_struct.fields().at("pos_infinity").has_number_value());
  EXPECT_TRUE(
      std::isinf(proto_struct.fields().at("pos_infinity").number_value()));
  EXPECT_GT(proto_struct.fields().at("pos_infinity").number_value(), 0);

  EXPECT_TRUE(proto_struct.fields().at("neg_infinity").has_number_value());
  EXPECT_TRUE(
      std::isinf(proto_struct.fields().at("neg_infinity").number_value()));
  EXPECT_LT(proto_struct.fields().at("neg_infinity").number_value(), 0);

  EXPECT_TRUE(proto_struct.fields().at("nan").has_number_value());
  EXPECT_TRUE(std::isnan(proto_struct.fields().at("nan").number_value()));

  EXPECT_TRUE(proto_struct.fields().at("max_number").has_number_value());
  EXPECT_EQ(proto_struct.fields().at("max_number").number_value(),
            std::numeric_limits<double>::max());

  EXPECT_TRUE(proto_struct.fields().at("min_number").has_number_value());
  EXPECT_EQ(proto_struct.fields().at("min_number").number_value(),
            std::numeric_limits<double>::min());

  EXPECT_TRUE(proto_struct.fields().at("empty_string").has_string_value());
  EXPECT_EQ(proto_struct.fields().at("empty_string").string_value(), "");
}

TEST_F(TypeConverterTest, V8ObjectToProtobufStructLargeObject) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);
  size_t num_fields = 100;
  nlohmann::json large_json;
  for (int i = 0; i < num_fields; i++) {
    large_json["key" + std::to_string(i)] = "value" + std::to_string(i);
  }

  std::string json_str = large_json.dump();

  v8::Local<v8::String> v8_json =
      v8::String::NewFromUtf8(isolate_, json_str.c_str()).ToLocalChecked();
  v8::Local<v8::Value> v8_obj_value;
  bool parsed = v8::JSON::Parse(context, v8_json).ToLocal(&v8_obj_value);
  ASSERT_TRUE(parsed) << "Failed to parse JSON";
  ASSERT_TRUE(v8_obj_value->IsObject()) << "Parsed value is not an object";

  v8::Local<v8::Object> v8_obj = v8_obj_value.As<v8::Object>();

  google::protobuf::Struct proto_struct;
  EXPECT_TRUE(TypeConverter<google::protobuf::Struct>::FromV8(isolate_, v8_obj,
                                                              &proto_struct));

  EXPECT_EQ(proto_struct.fields_size(), num_fields);

  for (int i = 0; i < num_fields; i++) {
    std::string key = "key" + std::to_string(i);
    std::string expected_value = "value" + std::to_string(i);

    ASSERT_TRUE(proto_struct.fields().contains(key));
    EXPECT_TRUE(proto_struct.fields().at(key).has_string_value());
    EXPECT_EQ(proto_struct.fields().at(key).string_value(), expected_value);
  }
}
}  // namespace google::scp::roma::config::test
