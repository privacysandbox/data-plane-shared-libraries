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

// Some code snippets are copied from V8 samples with the following license:
// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "include/libplatform/libplatform.h"
#include "include/v8-context.h"
#include "include/v8-initialization.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-primitive.h"
#include "include/v8-script.h"
#include "src/util/process_util.h"

using ::testing::StrEq;

namespace google::scp::roma::test {
class V8Test : public ::testing::Test {
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

  static v8::Platform* platform_;
  v8::Isolate::CreateParams create_params_;
  v8::Isolate* isolate_;
};

v8::Platform* V8Test::platform_{nullptr};

TEST_F(V8Test, BasicJs) {
  v8::Isolate::Scope isolate_scope(isolate_);

  // Create a stack-allocated handle scope.
  v8::HandleScope handle_scope(isolate_);

  // Create a new context.
  v8::Local<v8::Context> context = v8::Context::New(isolate_);

  // Enter the context for compiling and running the hello world script.
  v8::Context::Scope context_scope(context);
  // Create a string containing the JavaScript source code.
  v8::Local<v8::String> source =
      v8::String::NewFromUtf8Literal(isolate_, "'Hello' + ', World!'");

  // Compile the source code.
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, source).ToLocalChecked();

  // Run the script to get the result.
  v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

  // Convert the result to an UTF8 string.
  v8::String::Utf8Value utf8(isolate_, result);
  std::string val(*utf8, utf8.length());
  EXPECT_THAT(val, StrEq("Hello, World!"));
}

TEST_F(V8Test, BasicWasm) {
  v8::Isolate::Scope isolate_scope(isolate_);

  // Create a stack-allocated handle scope.
  v8::HandleScope handle_scope(isolate_);

  // Create a new context.
  v8::Local<v8::Context> context = v8::Context::New(isolate_);

  // Enter the context for compiling and running the hello world script.
  v8::Context::Scope context_scope(context);

  // Use the JavaScript API to generate a WebAssembly module.
  //
  // |bytes| contains the binary format for the following module:
  //
  //     (func (export "add") (param i32 i32) (result i32)
  //       get_local 0
  //       get_local 1
  //       i32.add)
  //
  const char csource[] = R"(
    let bytes = new Uint8Array([
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
      0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
      0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
      0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b
    ]);
    let module = new WebAssembly.Module(bytes);
    let instance = new WebAssembly.Instance(module);
    instance.exports.add(3, 4);
  )";

  // Create a string containing the JavaScript source code.
  v8::Local<v8::String> source =
      v8::String::NewFromUtf8Literal(isolate_, csource);

  // Compile the source code.
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, source).ToLocalChecked();

  // Run the script to get the result.
  v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

  uint32_t number = result->Uint32Value(context).ToChecked();
  EXPECT_EQ(number, 7);
}

}  // namespace google::scp::roma::test
