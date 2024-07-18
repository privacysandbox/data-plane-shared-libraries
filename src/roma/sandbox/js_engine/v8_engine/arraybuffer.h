/*
 * Copyright 2024 Google LLC
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

#ifndef ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_ARRAY_BUFFER_H_
#define ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_ARRAY_BUFFER_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/mman.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/strings/substitute.h"
#include "include/libplatform/libplatform.h"
#include "include/v8-context.h"
#include "include/v8-initialization.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-primitive.h"
#include "include/v8-script.h"
#include "src/util/process_util.h"

#include "arraybuffer_js.h"

namespace google::scp::roma::sandbox::js_engine::v8_js_engine::arraybuffer {

class Environment : public ::testing::Environment {
 public:
  ~Environment() override { TearDown(); }

  void SetUp() override {
    absl::StatusOr<std::string> my_path =
        ::privacy_sandbox::server_common::GetExePath();
    CHECK_OK(my_path);
    v8::V8::InitializeICUDefaultLocation(my_path->data());
    v8::V8::InitializeExternalStartupData(my_path->data());

    platform_ = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform_.get());
    v8::V8::Initialize();
  }

  void TearDown() override {
    if (platform_) {
      v8::V8::Dispose();
      v8::V8::DisposePlatform();
      platform_.reset();
    }
  }

 private:
  std::unique_ptr<v8::Platform> platform_;
};

// A LocalContext holds a reference to a v8::Context.
class LocalContext {
 public:
  LocalContext(v8::Isolate* isolate,
               v8::ExtensionConfiguration* extensions = nullptr,
               v8::Local<v8::ObjectTemplate> global_template =
                   v8::Local<v8::ObjectTemplate>(),
               v8::Local<v8::Value> global_object = v8::Local<v8::Value>()) {
    Initialize(isolate, extensions, global_template, global_object);
  }

  virtual ~LocalContext() {
    v8::HandleScope scope(isolate_);
    v8::Local<v8::Context>::New(isolate_, context_)->Exit();
    context_.Reset();
  }

  v8::Context* operator->() {
    return *reinterpret_cast<v8::Context**>(&context_);
  }
  v8::Context* operator*() { return operator->(); }
  bool IsReady() { return !context_.IsEmpty(); }

  v8::Local<v8::Context> local() const {
    return v8::Local<v8::Context>::New(isolate_, context_);
  }

 private:
  void Initialize(v8::Isolate* isolate, v8::ExtensionConfiguration* extensions,
                  v8::Local<v8::ObjectTemplate> global_template,
                  v8::Local<v8::Value> global_object) {
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context =
        v8::Context::New(isolate, extensions, global_template, global_object);
    context_.Reset(isolate, context);
    context->Enter();
    // We can't do this later perhaps because of a fatal error.
    isolate_ = isolate;
  }

  v8::Persistent<v8::Context> context_;
  v8::Isolate* isolate_;
};

inline v8::Local<v8::String> v8_str(v8::Isolate* isolate,
                                    std::string_view str) {
  return v8::String::NewFromUtf8(isolate, str.data(),
                                 v8::NewStringType::kNormal, str.size())
      .ToLocalChecked();
}

inline v8::Local<v8::String> v8_str(v8::Local<v8::Context> context,
                                    std::string_view str) {
  return v8_str(context->GetIsolate(), str);
}

// Helper functions that compile and run the source.
inline v8::MaybeLocal<v8::Value> CompileRun(v8::Local<v8::Context> context,
                                            std::string_view source) {
  return v8::Script::Compile(context, v8_str(context, source))
      .ToLocalChecked()
      ->Run(context);
}

inline void ExpectString(v8::Local<v8::Context> context, std::string_view code,
                         std::string_view expected) {
  v8::MaybeLocal<v8::Value> result = CompileRun(context, code);
  if (result.IsEmpty()) {
    CHECK(expected.empty());
    return;
  }
  CHECK(result.ToLocalChecked()->IsString());
  const auto actual =
      result.ToLocalChecked()->ToString(context).ToLocalChecked();
  ASSERT_TRUE(actual->StringEquals(v8_str(context, expected)))
      << " expected: " << expected
      << ", actual: " << *v8::String::Utf8Value(context->GetIsolate(), actual);
}

inline int64_t GetInt64(v8::Local<v8::Context> context, std::string_view code) {
  v8::MaybeLocal<v8::Value> result = CompileRun(context, code);
  if (result.IsEmpty()) {
    return -1;
  }
  CHECK(result.ToLocalChecked()->IsNumber());
  return result.ToLocalChecked()->IntegerValue(context).FromJust();
}

inline void ExpectInt64(v8::Local<v8::Context> context, std::string_view code,
                        int32_t expected) {
  const int64_t actual = GetInt64(context, code);
  ASSERT_EQ(expected, actual)
      << " expected: " << expected << ", actual: " << actual;
}

inline void MMapDeleter(void* data, size_t length, void* deleter_data) {
  PCHECK(::munmap(deleter_data, length) != -1);
}

inline void MapFlatbufferFile(LocalContext& env,
                              std::filesystem::path mmap_path,
                              std::string_view data_layout,
                              std::string_view arraybuffer_var_name,
                              std::string_view fbuffer_var_name,
                              size_t& data_len) {
  CHECK(std::filesystem::is_regular_file(mmap_path))
      << "mmap file must be a regular file";
  std::error_code ec;
  data_len = std::filesystem::file_size(mmap_path, ec);
  CHECK(!ec) << ec.message();
  const int fd = ::open(mmap_path.c_str(), O_RDONLY);
  PCHECK(fd != -1) << "mmap file cannot be opened";
  void* mmdata = ::mmap(nullptr, data_len, PROT_READ, MAP_SHARED, fd, 0);
  PCHECK(mmdata != reinterpret_cast<void*>(-1)) << "mmap failed";
  PCHECK(::close(fd) == 0);

  uint8_t* store_ptr =
      reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(mmdata));

  std::shared_ptr<v8::BackingStore> bstore = v8::ArrayBuffer::NewBackingStore(
      store_ptr, data_len, MMapDeleter, mmdata);
  v8::Isolate* isolate = env->GetIsolate();
  const auto external_mem = isolate->AdjustAmountOfExternalAllocatedMemory(0);
  v8::Local<v8::ArrayBuffer> array_buffer =
      v8::ArrayBuffer::New(isolate, std::move(bstore));
  CHECK(isolate->AdjustAmountOfExternalAllocatedMemory(0) >= external_mem);

  auto backing_store = array_buffer->GetBackingStore();
  ASSERT_EQ(data_len, backing_store->ByteLength());
  ASSERT_FALSE(backing_store->IsShared());
  ASSERT_FALSE(backing_store->IsResizableByUserJavaScript());
  ASSERT_EQ(store_ptr, array_buffer->Data());

  v8::Local<v8::Context> ctx = env.local();
  ASSERT_TRUE(env->Global()
                  ->Set(ctx, v8_str(ctx, arraybuffer_var_name), array_buffer)
                  .FromJust());

  (void)CompileRun(
      ctx, absl::Substitute("const $3 = CreateFbufferKVStruct('$0', $1, $2);",
                            data_layout, arraybuffer_var_name, data_len,
                            fbuffer_var_name));
}

}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine::arraybuffer

#endif  // ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_ARRAY_BUFFER_H_
