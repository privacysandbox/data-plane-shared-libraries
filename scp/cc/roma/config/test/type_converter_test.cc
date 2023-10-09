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

#include "roma/config/src/type_converter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <libplatform/libplatform.h>

#include "absl/container/flat_hash_map.h"
#include "include/v8.h"
#include "roma/common/src/containers.h"
#include "scp/cc/roma/interface/function_binding_io.pb.h"

namespace google::scp::roma::config::test {
class TypeConverterTest : public ::testing::Test {
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

v8::Platform* TypeConverterTest::platform_{nullptr};

static void AssertStringEquality(v8::Isolate* isolate,
                                 const std::string& native_str,
                                 const v8::Local<v8::String>& v8_str) {
  EXPECT_EQ(native_str.size(), v8_str->Length());
  EXPECT_EQ(
      0, strcmp(*v8::String::Utf8Value(isolate, v8_str), native_str.c_str()));
}

TEST_F(TypeConverterTest, NativeStringToV8) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);

  std::string native_str = "I am a string";

  v8::Local<v8::String> v8_str =
      TypeConverter<std::string>::ToV8(isolate_, native_str).As<v8::String>();

  AssertStringEquality(isolate_, native_str, v8_str);
}

TEST_F(TypeConverterTest, v8StringToNative) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);

  v8::Local<v8::String> v8_str =
      v8::String::NewFromUtf8(isolate_, "I am a string").ToLocalChecked();

  std::string native_str;
  EXPECT_TRUE(
      TypeConverter<std::string>::FromV8(isolate_, v8_str, &native_str));

  AssertStringEquality(isolate_, native_str, v8_str);
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

static void AssertArrayEquality(v8::Isolate* isolate,
                                const std::vector<std::string>& vec,
                                const v8::Local<v8::Array>& v8_array) {
  EXPECT_EQ(vec.size(), v8_array->Length());

  for (uint32_t i = 0; i < v8_array->Length(); i++) {
    auto v8_array_item = v8_array->Get(isolate->GetCurrentContext(), i)
                             .ToLocalChecked()
                             .As<v8::String>();
    AssertStringEquality(isolate, vec.at(i), v8_array_item);
  }
}

TEST_F(TypeConverterTest, VectorOfStringToV8Array) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  // Array allocation requires a context
  v8::Local<v8::Context> global_context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(global_context);

  const std::vector<std::string> vec = {"one", "two", "three"};

  const v8::Local<v8::Array> v8_array =
      TypeConverter<std::vector<std::string>>::ToV8(isolate_, vec)
          .As<v8::Array>();

  AssertArrayEquality(isolate_, vec, v8_array);
}

TEST_F(TypeConverterTest, ListOfStringProtoToV8Array) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  // Array allocation requires a context
  v8::Local<v8::Context> global_context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(global_context);

  const std::vector<std::string> kv_vec = {"one", "two", "three"};
  proto::FunctionBindingIoProto io_proto;
  (*io_proto.mutable_output_list_of_string()->mutable_data())
      .Add(kv_vec.begin(), kv_vec.end());

  const v8::Local<v8::Array> v8_array =
      TypeConverter<std::vector<std::string>>::ToV8(
          isolate_, io_proto.output_list_of_string().data())
          .As<v8::Array>();

  AssertArrayEquality(isolate_, kv_vec, v8_array);
}

TEST_F(TypeConverterTest, v8ArrayToVectorOfString) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  // Array allocation requires a context
  v8::Local<v8::Context> global_context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(global_context);

  v8::Local<v8::Array> v8_array = v8::Array::New(isolate_, 3);
  v8_array
      ->Set(global_context, 0, v8::String::NewFromUtf8Literal(isolate_, "one"))
      .Check();
  v8_array
      ->Set(global_context, 1, v8::String::NewFromUtf8Literal(isolate_, "two"))
      .Check();
  v8_array
      ->Set(global_context, 2,
            v8::String::NewFromUtf8Literal(isolate_, "three"))
      .Check();

  std::vector<std::string> vec;
  EXPECT_TRUE(TypeConverter<std::vector<std::string>>::FromV8(isolate_,
                                                              v8_array, &vec));

  AssertArrayEquality(isolate_, vec, v8_array);
}

TEST_F(TypeConverterTest, v8ArrayToVectorOfStringFailsWhenUnsupportedType) {
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

TEST_F(TypeConverterTest,
       v8ArrayToVectorOfStringFailsWhenUnsupportedMixedTypes) {
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

  EXPECT_TRUE(vec.empty());
}

static void AssertMapOfStringEquality(
    v8::Isolate* isolate, common::Map<std::string, std::string>& map,
    v8::Local<v8::Map>& v8_map) {
  EXPECT_EQ(map.Size(), v8_map->Size());

  // This turns the map into an array of size Size()*2, where index N is a key,
  // and N+1 is the value for the given key.
  auto v8_map_as_array = v8_map->AsArray();
  auto native_map_keys = map.Keys();

  int j = 0;
  for (size_t i = 0; i < v8_map_as_array->Length(); i += 2) {
    auto key_index = i;
    auto value_index = i + 1;

    auto v8_key = v8_map_as_array->Get(isolate->GetCurrentContext(), key_index)
                      .ToLocalChecked()
                      .As<v8::String>();
    auto v8_val =
        v8_map_as_array->Get(isolate->GetCurrentContext(), value_index)
            .ToLocalChecked()
            .As<v8::String>();

    auto native_key = native_map_keys.at(j++);
    auto native_val = map.Get(native_key);

    AssertStringEquality(isolate, native_key, v8_key);
    AssertStringEquality(isolate, native_val, v8_val);
  }
}

static void AssertFlatHashMapOfStringEquality(
    v8::Isolate* isolate,
    const absl::flat_hash_map<std::string, std::string>& map,
    const v8::Local<v8::Map>& v8_map) {
  EXPECT_EQ(map.size(), v8_map->Size());

  // This turns the map into an array of size Size()*2, where index N is a key,
  // and N+1 is the value for the given key.
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
    auto key_index = i;
    auto value_index = i + 1;

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

  EXPECT_THAT(native_map_keys, ::testing::ElementsAreArray(v8_map_keys));
  EXPECT_THAT(native_map_vals, ::testing::ElementsAreArray(v8_map_vals));
}

TEST_F(TypeConverterTest, MapOfStringStringToV8Map) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  // Array allocation requires a context
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  common::Map<std::string, std::string> map;
  map.Set("key1", "val1");
  map.Set("key2", "val2");
  map.Set("key3", "val3");

  v8::Local<v8::Map> v8_map =
      TypeConverter<common::Map<std::string, std::string>>::ToV8(isolate_, map)
          .As<v8::Map>();

  AssertMapOfStringEquality(isolate_, map, v8_map);
}

TEST_F(TypeConverterTest, v8MapToMapOfStringString) {
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

  common::Map<std::string, std::string> map;
  EXPECT_TRUE((TypeConverter<common::Map<std::string, std::string>>::FromV8(
      isolate_, v8_map, &map)));

  AssertMapOfStringEquality(isolate_, map, v8_map);
}

TEST_F(TypeConverterTest, FlatHashMapOfStringStringToV8Map) {
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

  const v8::Local<v8::Map> v8_map =
      TypeConverter<absl::flat_hash_map<std::string, std::string>>::ToV8(
          isolate_, kv_map)
          .As<v8::Map>();

  AssertFlatHashMapOfStringEquality(isolate_, kv_map, v8_map);
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
       v8MapToMapOfStringStringShouldFailWithUnsupportedTypeVal) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  // Array allocation requires a context
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Map> v8_map = v8::Map::New(isolate_);
  v8_map
      ->Set(context, v8::String::NewFromUtf8Literal(isolate_, "key1"),
            v8::Number::New(isolate_, 1))
      .ToLocalChecked();

  common::Map<std::string, std::string> map;
  EXPECT_FALSE((TypeConverter<common::Map<std::string, std::string>>::FromV8(
      isolate_, v8_map, &map)));

  EXPECT_EQ(map.Size(), 0);
}

TEST_F(TypeConverterTest,
       v8MapToFlatHashMapOfStringStringShouldFailWithUnsupportedTypeVal) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  // Array allocation requires a context
  v8::Local<v8::Context> context = v8::Context::New(isolate_);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Map> v8_map = v8::Map::New(isolate_);
  v8_map
      ->Set(context, v8::String::NewFromUtf8Literal(isolate_, "key1"),
            v8::Number::New(isolate_, 1))
      .ToLocalChecked();

  absl::flat_hash_map<std::string, std::string> map;
  EXPECT_FALSE(
      (TypeConverter<absl::flat_hash_map<std::string, std::string>>::FromV8(
          isolate_, v8_map, &map)));

  EXPECT_EQ(map.size(), 0);
}

TEST_F(TypeConverterTest,
       v8MapToMapOfStringStringShouldFailWithUnsupportedTypeKey) {
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

  common::Map<std::string, std::string> map;
  EXPECT_FALSE((TypeConverter<common::Map<std::string, std::string>>::FromV8(
      isolate_, v8_map, &map)));

  EXPECT_EQ(map.Size(), 0);
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

  EXPECT_EQ(map.size(), 0);
}

TEST_F(TypeConverterTest,
       v8MapToMapOfStringStringShouldFailWithUnsupportedMixedTypes) {
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

  common::Map<std::string, std::string> map;
  EXPECT_FALSE((TypeConverter<common::Map<std::string, std::string>>::FromV8(
      isolate_, v8_map, &map)));

  EXPECT_EQ(map.Size(), 0);
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

  EXPECT_EQ(map.size(), 0);
}

TEST_F(TypeConverterTest, NativeUint32ToV8) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);

  uint32_t native_val = 1234;

  v8::Local<v8::Uint32> v8_val =
      TypeConverter<uint32_t>::ToV8(isolate_, native_val).As<v8::Uint32>();

  EXPECT_EQ(native_val, v8_val->Value());
}

TEST_F(TypeConverterTest, v8Uint32ToNative) {
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);

  auto v8_val = v8::Integer::NewFromUnsigned(isolate_, 4567);

  uint32_t native_val;
  EXPECT_TRUE(TypeConverter<uint32_t>::FromV8(isolate_, v8_val, &native_val));

  EXPECT_EQ(4567, native_val);
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
  EXPECT_EQ(native_val.size(), v8_val->Length());

  // Compare the actual values
  for (int i = 0; i < native_val.size(); i++) {
    const auto val = v8_val->Get(global_context, i).ToLocalChecked();
    uint32_t native_int;
    TypeConverter<uint32_t>::FromV8(isolate_, val, &native_int);
    EXPECT_EQ(native_val.at(i), native_int);
  }

  // The above should be enough, but also compare the buffers to be thorough
  EXPECT_EQ(
      0, memcmp(native_val.data(), v8_val->Buffer()->Data(), v8_val->Length()));
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
    EXPECT_EQ(out_data.get()[i], native_int);
  }

  // The above should be enough, but also compare the buffers to be thorough
  EXPECT_EQ(0,
            memcmp(out_data.get(), v8_val->Buffer()->Data(), v8_val->Length()));
}
}  // namespace google::scp::roma::config::test
