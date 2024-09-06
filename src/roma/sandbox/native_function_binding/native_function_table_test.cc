/*
 * Copyright 2023 Google LLC
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

#include "src/roma/sandbox/native_function_binding/native_function_table.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "src/roma/interface/function_binding_io.pb.h"

namespace google::scp::roma::sandbox::native_function_binding::test {

void ExampleFunction(FunctionBindingPayload<>& payload) {}

TEST(NativeFunctionTableTest, RegisterPasses) {
  NativeFunctionTable table;
  EXPECT_TRUE(table.Register("example", ExampleFunction).ok());
}

TEST(NativeFunctionTableTest, RegisterTwiceFails) {
  NativeFunctionTable table;
  EXPECT_TRUE(table.Register("example", ExampleFunction).ok());
  EXPECT_FALSE(table.Register("example", ExampleFunction).ok());
}

TEST(NativeFunctionTableTest, RegisterClearRegisterPasses) {
  NativeFunctionTable table;
  EXPECT_TRUE(table.Register("example", ExampleFunction).ok());
  table.Clear();
  EXPECT_TRUE(table.Register("example", ExampleFunction).ok());
}

TEST(NativeFunctionTableTest, CallRegisteredFunction) {
  NativeFunctionTable table;
  EXPECT_TRUE(table.Register("example", ExampleFunction).ok());
  proto::FunctionBindingIoProto input;
  FunctionBindingPayload<> payload = {
      .io_proto = input,
      .metadata = {},
  };
  EXPECT_TRUE(table.Call("example", payload).ok());
}

TEST(NativeFunctionTableTest, CallUnregisteredFunction) {
  NativeFunctionTable table;
  proto::FunctionBindingIoProto input;
  FunctionBindingPayload<> payload = {
      .io_proto = input,
      .metadata = {},
  };
  EXPECT_FALSE(table.Call("example", payload).ok());
}

}  // namespace google::scp::roma::sandbox::native_function_binding::test
