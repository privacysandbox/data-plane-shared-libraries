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

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#include "absl/container/flat_hash_map.h"
#include "include/libplatform/libplatform.h"
#include "include/v8.h"
#include "src/roma/config/type_converter.h"
#include "src/roma/interface/function_binding_io.pb.h"

using ::google::scp::roma::TypeConverter;
using ::google::scp::roma::proto::FunctionBindingIoProto;
using ::testing::ElementsAreArray;
using ::testing::StrEq;

namespace {

// Utility class to handle cleaning up V8 objects.
class V8Deleter {
 public:
  V8Deleter() {
    create_params_.array_buffer_allocator =
        v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    isolate_ = v8::Isolate::New(create_params_);
  }

  ~V8Deleter() {
    isolate_->Dispose();
    delete create_params_.array_buffer_allocator;
  }

  v8::Isolate* isolate() { return isolate_; }

 private:
  v8::Isolate::CreateParams create_params_;
  v8::Isolate* isolate_;
};

void BM_NativeStringToV8(benchmark::State& state) {
  V8Deleter deleter;
  const std::string native_str = "I am a string";
  {
    // These V8 objects have to go out of scope before the V8Deleter does.
    v8::Isolate::Scope isolate_scope(deleter.isolate());
    v8::HandleScope handle_scope(deleter.isolate());
    for (auto _ : state) {
      benchmark::DoNotOptimize(
          TypeConverter<std::string>::ToV8(deleter.isolate(), native_str)
              .As<v8::String>());
    }
  }
}

void BM_v8StringToNative(benchmark::State& state) {
  V8Deleter deleter;
  {
    // These V8 objects have to go out of scope before the V8Deleter does.
    v8::Isolate::Scope isolate_scope(deleter.isolate());
    v8::HandleScope handle_scope(deleter.isolate());

    v8::Local<v8::String> v8_str =
        v8::String::NewFromUtf8(deleter.isolate(), "I am a string")
            .ToLocalChecked();

    for (auto _ : state) {
      std::string native_str;
      benchmark::DoNotOptimize(TypeConverter<std::string>::FromV8(
          deleter.isolate(), v8_str, &native_str));
    }
  }
}

void BM_ListOfStringProtoToV8Array(benchmark::State& state) {
  V8Deleter deleter;
  {
    // These V8 objects have to go out of scope before the V8Deleter does.
    v8::Isolate::Scope isolate_scope(deleter.isolate());
    v8::HandleScope handle_scope(deleter.isolate());

    v8::Local<v8::Context> global_context = v8::Context::New(deleter.isolate());
    v8::Context::Scope context_scope(global_context);

    const std::vector<std::string> kv_vec = {"one", "two", "three"};

    FunctionBindingIoProto io_proto;
    (*io_proto.mutable_output_list_of_string()->mutable_data())
        .Add(kv_vec.begin(), kv_vec.end());

    for (auto _ : state) {
      benchmark::DoNotOptimize(
          TypeConverter<std::vector<std::string>>::ToV8(
              deleter.isolate(), io_proto.output_list_of_string().data())
              .As<v8::Array>());
    }
  }
}

void BM_v8ArrayToVectorOfString(benchmark::State& state) {
  V8Deleter deleter;
  {
    // These V8 objects have to go out of scope before the V8Deleter does.
    v8::Isolate::Scope isolate_scope(deleter.isolate());
    v8::HandleScope handle_scope(deleter.isolate());

    v8::Local<v8::Context> global_context = v8::Context::New(deleter.isolate());
    v8::Context::Scope context_scope(global_context);

    v8::Local<v8::Array> v8_array = v8::Array::New(deleter.isolate(), 3);
    v8_array
        ->Set(global_context, 0,
              v8::String::NewFromUtf8Literal(deleter.isolate(), "one"))
        .Check();
    v8_array
        ->Set(global_context, 1,
              v8::String::NewFromUtf8Literal(deleter.isolate(), "two"))
        .Check();
    v8_array
        ->Set(global_context, 2,
              v8::String::NewFromUtf8Literal(deleter.isolate(), "three"))
        .Check();

    for (auto _ : state) {
      std::vector<std::string> vec;
      benchmark::DoNotOptimize(TypeConverter<std::vector<std::string>>::FromV8(
          deleter.isolate(), v8_array, &vec));
    }
  }
}

void BM_MapOfStringStringProtoToV8Map(benchmark::State& state) {
  V8Deleter deleter;
  {
    // These V8 objects have to go out of scope before the V8Deleter does.
    v8::Isolate::Scope isolate_scope(deleter.isolate());
    v8::HandleScope handle_scope(deleter.isolate());

    v8::Local<v8::Context> global_context = v8::Context::New(deleter.isolate());
    v8::Context::Scope context_scope(global_context);

    const absl::flat_hash_map<std::string, std::string> kv_map = {
        {"key1", "val1"},
        {"key2", "val2"},
        {"key3", "val3"},
    };

    FunctionBindingIoProto io_proto;
    (*io_proto.mutable_output_map_of_string()->mutable_data())
        .insert(kv_map.begin(), kv_map.end());

    for (auto _ : state) {
      std::vector<std::string> vec;
      benchmark::DoNotOptimize(
          TypeConverter<absl::flat_hash_map<std::string, std::string>>::ToV8(
              deleter.isolate(), io_proto.output_map_of_string().data())
              .As<v8::Map>());
    }
  }
}

void BM_v8MapToFlatHashMapOfStringString(benchmark::State& state) {
  V8Deleter deleter;
  {
    // These V8 objects have to go out of scope before the V8Deleter does.
    v8::Isolate::Scope isolate_scope(deleter.isolate());
    v8::HandleScope handle_scope(deleter.isolate());

    v8::Local<v8::Context> global_context = v8::Context::New(deleter.isolate());
    v8::Context::Scope context_scope(global_context);

    v8::Local<v8::Map> v8_map = v8::Map::New(deleter.isolate());
    v8_map
        ->Set(global_context,
              v8::String::NewFromUtf8Literal(deleter.isolate(), "key1"),
              v8::String::NewFromUtf8Literal(deleter.isolate(), "val1"))
        .ToLocalChecked();
    v8_map
        ->Set(global_context,
              v8::String::NewFromUtf8Literal(deleter.isolate(), "key2"),
              v8::String::NewFromUtf8Literal(deleter.isolate(), "val2"))
        .ToLocalChecked();
    v8_map
        ->Set(global_context,
              v8::String::NewFromUtf8Literal(deleter.isolate(), "key3"),
              v8::String::NewFromUtf8Literal(deleter.isolate(), "val3"))
        .ToLocalChecked();

    for (auto _ : state) {
      absl::flat_hash_map<std::string, std::string> map;
      benchmark::DoNotOptimize(
          TypeConverter<absl::flat_hash_map<std::string, std::string>>::FromV8(
              deleter.isolate(), v8_map, &map));
    }
  }
}

void BM_NativeUint32ToV8(benchmark::State& state) {
  V8Deleter deleter;
  {
    // These V8 objects have to go out of scope before the V8Deleter does.
    v8::Isolate::Scope isolate_scope(deleter.isolate());
    v8::HandleScope handle_scope(deleter.isolate());

    constexpr uint32_t native_val = 1234;

    for (auto _ : state) {
      benchmark::DoNotOptimize(
          TypeConverter<uint32_t>::ToV8(deleter.isolate(), native_val)
              .As<v8::Uint32>());
    }
  }
}

void BM_v8Uint32ToNative(benchmark::State& state) {
  V8Deleter deleter;
  {
    // These V8 objects have to go out of scope before the V8Deleter does.
    v8::Isolate::Scope isolate_scope(deleter.isolate());
    v8::HandleScope handle_scope(deleter.isolate());

    auto v8_val = v8::Integer::NewFromUnsigned(deleter.isolate(), 4567);

    for (auto _ : state) {
      uint32_t native_val;
      benchmark::DoNotOptimize(TypeConverter<uint32_t>::FromV8(
          deleter.isolate(), v8_val, &native_val));
    }
  }
}

void BM_NativeUint8PointerToV8(benchmark::State& state) {
  V8Deleter deleter;
  {
    // These V8 objects have to go out of scope before the V8Deleter does.
    v8::Isolate::Scope isolate_scope(deleter.isolate());
    v8::HandleScope handle_scope(deleter.isolate());

    v8::Local<v8::Context> global_context = v8::Context::New(deleter.isolate());
    v8::Context::Scope context_scope(global_context);

    const std::vector<uint8_t> native_val = {1, 2, 3, 4};

    for (auto _ : state) {
      benchmark::DoNotOptimize(TypeConverter<uint8_t*>::ToV8(deleter.isolate(),
                                                             native_val.data(),
                                                             native_val.size())
                                   .As<v8::Uint8Array>());
    }
  }
}

void BM_V8Uint8ArrayToNativeUint8Pointer(benchmark::State& state) {
  V8Deleter deleter;
  {
    // These V8 objects have to go out of scope before the V8Deleter does.
    v8::Isolate::Scope isolate_scope(deleter.isolate());
    v8::HandleScope handle_scope(deleter.isolate());

    v8::Local<v8::Context> global_context = v8::Context::New(deleter.isolate());
    v8::Context::Scope context_scope(global_context);

    constexpr int data_size = 3;
    auto buffer = v8::ArrayBuffer::New(deleter.isolate(), data_size);
    auto v8_val = v8::Uint8Array::New(buffer, 0, data_size);
    v8_val->Set(global_context, 0, v8::Integer::New(deleter.isolate(), 3))
        .Check();
    v8_val->Set(global_context, 1, v8::Integer::New(deleter.isolate(), 2))
        .Check();
    v8_val->Set(global_context, 2, v8::Integer::New(deleter.isolate(), 1))
        .Check();

    for (auto _ : state) {
      auto out_data = std::make_unique<uint8_t[]>(data_size);
      benchmark::DoNotOptimize(TypeConverter<uint8_t*>::FromV8(
          deleter.isolate(), v8_val, out_data.get(), data_size));
    }
  }
}

}  // namespace

BENCHMARK(BM_NativeStringToV8);
BENCHMARK(BM_v8StringToNative);
BENCHMARK(BM_ListOfStringProtoToV8Array);
BENCHMARK(BM_v8ArrayToVectorOfString);
BENCHMARK(BM_MapOfStringStringProtoToV8Map);
BENCHMARK(BM_v8MapToFlatHashMapOfStringString);
BENCHMARK(BM_NativeUint32ToV8);
BENCHMARK(BM_v8Uint32ToNative);
BENCHMARK(BM_NativeUint8PointerToV8);
BENCHMARK(BM_V8Uint8ArrayToNativeUint8Pointer);

// Run the benchmarks
int main(int argc, char* argv[]) {
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  v8::V8::Dispose();
  v8::V8::DisposePlatform();
  return 0;
}
