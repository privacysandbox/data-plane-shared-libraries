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

#include "src/roma/sandbox/worker_api/sapi/worker_sandbox_api.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <signal.h>

#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "sandboxed_api/sandbox2/comms.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/native_function_binding/rpc_wrapper.pb.h"
#include "src/roma/sandbox/worker_api/sapi/utils.h"

using google::scp::roma::proto::RpcWrapper;
using google::scp::roma::sandbox::constants::kCodeVersion;
using google::scp::roma::sandbox::constants::kHandlerName;
using google::scp::roma::sandbox::constants::kRequestAction;
using google::scp::roma::sandbox::constants::kRequestActionExecute;
using google::scp::roma::sandbox::constants::kRequestType;
using google::scp::roma::sandbox::constants::kRequestTypeJavascript;
using ::testing::HasSubstr;
using ::testing::StrEq;

namespace google::scp::roma::sandbox::worker_api::test {
TEST(WorkerSandboxApiTest, WorkerWorksThroughSandbox) {
  WorkerSandboxApi sandbox_api(
      /*require_preload=*/false, /*native_js_function_comms_fd=*/-1,
      /*native_js_function_names=*/{}, /*rpc_method_names=*/{},
      /*server_address=*/"", /*max_worker_virtual_memory_mb=*/0,
      /*js_engine_initial_heap_size_mb=*/0,
      /*js_engine_maximum_heap_size_mb=*/0,
      /*js_engine_max_wasm_memory_number_of_pages=*/0,
      /*sandbox_request_response_shared_buffer_size_mb=*/0,
      /*enable_sandbox_sharing_request_response_with_buffer_only=*/false,
      /*v8_flags=*/{},
      /*enable_profilers=*/false,
      /*logging_function_set=*/false,
      /*disable_udf_stacktraces_in_response=*/false);

  ASSERT_TRUE(sandbox_api.Init().ok());

  ASSERT_TRUE(sandbox_api.Run().ok());

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(
      R"js(function cool_func() { return "Hi there from sandboxed JS :)" })js");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  std::pair<absl::Status, RetryStatus> result_pair =
      sandbox_api.RunCode(params_proto);
  ASSERT_TRUE(result_pair.first.ok());
  EXPECT_EQ(result_pair.second, RetryStatus::kDoNotRetry);
  EXPECT_THAT(params_proto.response(),
              StrEq(R"js("Hi there from sandboxed JS :)")js"));

  EXPECT_TRUE(sandbox_api.Stop().ok());
}

TEST(WorkerSandboxApiTest, WorkerReturnsInformativeThrowMessageThroughSandbox) {
  WorkerSandboxApi sandbox_api(
      /*require_preload=*/false, /*native_js_function_comms_fd=*/-1,
      /*native_js_function_names=*/{}, /*rpc_method_names=*/{},
      /*server_address=*/"",
      /*max_worker_virtual_memory_mb=*/0, /*js_engine_initial_heap_size_mb=*/0,
      /*js_engine_maximum_heap_size_mb=*/0,
      /*js_engine_max_wasm_memory_number_of_pages=*/0,
      /*sandbox_request_response_shared_buffer_size_mb=*/0,
      /*enable_sandbox_sharing_request_response_with_buffer_only=*/false,
      /*v8_flags=*/{}, /*enable_profilers=*/false,
      /*logging_function_set=*/false,
      /*disable_udf_stacktraces_in_response=*/false);

  ASSERT_TRUE(sandbox_api.Init().ok());

  ASSERT_TRUE(sandbox_api.Run().ok());

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(
      R"js(function cool_func() { throw new Error("Throw check!") })js");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  std::pair<absl::Status, RetryStatus> result_pair =
      sandbox_api.RunCode(params_proto);
  ASSERT_TRUE(!result_pair.first.ok());
  EXPECT_THAT(result_pair.first.message(),
              HasSubstr("Execution failed; Error when invoking the handler. "
                        "Uncaught Error: Throw check!"));
  EXPECT_TRUE(sandbox_api.Stop().ok());
}

TEST(WorkerSandboxApiTest, WorkerReturnsInformativeMessageForMissingParam) {
  WorkerSandboxApi sandbox_api(
      /*require_preload=*/false, /*native_js_function_comms_fd=*/-1,
      /*native_js_function_names=*/{}, /*rpc_method_names=*/{},
      /*server_address=*/"", /*max_worker_virtual_memory_mb=*/0,
      /*js_engine_initial_heap_size_mb=*/0,
      /*js_engine_maximum_heap_size_mb=*/0,
      /*js_engine_max_wasm_memory_number_of_pages=*/0,
      /*sandbox_request_response_shared_buffer_size_mb=*/0,
      /*enable_sandbox_sharing_request_response_with_buffer_only=*/false,
      /*v8_flags=*/{}, /*enable_cpu_profiler=*/false,
      /*logging_function_set=*/false,
      /*disable_udf_stacktraces_in_response=*/false);

  ASSERT_TRUE(sandbox_api.Init().ok());

  ASSERT_TRUE(sandbox_api.Run().ok());

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(R"js(function cool_func() { return config.name; })js");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  std::pair<absl::Status, RetryStatus> result_pair =
      sandbox_api.RunCode(params_proto);
  ASSERT_TRUE(!result_pair.first.ok());
  EXPECT_THAT(result_pair.first.message(),
              HasSubstr("Execution failed; Error when invoking the handler. "
                        "Uncaught ReferenceError: config is not defined"));
  EXPECT_TRUE(sandbox_api.Stop().ok());
}
}  // namespace google::scp::roma::sandbox::worker_api::test
