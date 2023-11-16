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

#include "roma/sandbox/native_function_binding/src/native_function_table.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "public/core/test/interface/execution_result_matchers.h"
#include "scp/cc/roma/interface/function_binding_io.pb.h"

namespace google::scp::roma::sandbox::native_function_binding::test {

void ExampleFunction(proto::FunctionBindingIoProto& io_proto) {}

TEST(NativeFunctionTableTest, RegisterPasses) {
  NativeFunctionTable table;
  EXPECT_SUCCESS(table.Register("example", ExampleFunction));
}

TEST(NativeFunctionTableTest, RegisterTwiceFails) {
  NativeFunctionTable table;
  EXPECT_SUCCESS(table.Register("example", ExampleFunction));
  EXPECT_FALSE(table.Register("example", ExampleFunction).Successful());
}

TEST(NativeFunctionTableTest, RegisterClearRegisterPasses) {
  NativeFunctionTable table;
  EXPECT_SUCCESS(table.Register("example", ExampleFunction));
  table.Clear();
  EXPECT_SUCCESS(table.Register("example", ExampleFunction));
}

TEST(NativeFunctionTableTest, CallRegisteredFunction) {
  NativeFunctionTable table;
  EXPECT_SUCCESS(table.Register("example", ExampleFunction));
  proto::FunctionBindingIoProto input;
  EXPECT_SUCCESS(table.Call("example", input));
}

TEST(NativeFunctionTableTest, CallUnregisteredFunction) {
  NativeFunctionTable table;
  proto::FunctionBindingIoProto input;
  EXPECT_FALSE(table.Call("example", input).Successful());
}

}  // namespace google::scp::roma::sandbox::native_function_binding::test
