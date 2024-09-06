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

#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/native_function_binding/rpc_wrapper.pb.h"

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
      /*v8_flags=*/
      {});

  ASSERT_TRUE(sandbox_api.Init().ok());

  ASSERT_TRUE(sandbox_api.Run().ok());

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(
      R"js(function cool_func() { return "Hi there from sandboxed JS :)" })js");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  std::pair<absl::Status, WorkerSandboxApi::RetryStatus> result_pair =
      sandbox_api.RunCode(params_proto);
  ASSERT_TRUE(result_pair.first.ok());
  EXPECT_EQ(result_pair.second, WorkerSandboxApi::RetryStatus::kDoNotRetry);
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
      /*v8_flags=*/{});

  ASSERT_TRUE(sandbox_api.Init().ok());

  ASSERT_TRUE(sandbox_api.Run().ok());

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(
      R"js(function cool_func() { throw new Error("Throw check!") })js");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  std::pair<absl::Status, WorkerSandboxApi::RetryStatus> result_pair =
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
      /*v8_flags=*/{});

  ASSERT_TRUE(sandbox_api.Init().ok());

  ASSERT_TRUE(sandbox_api.Run().ok());

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(R"js(function cool_func() { return config.name; })js");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  std::pair<absl::Status, WorkerSandboxApi::RetryStatus> result_pair =
      sandbox_api.RunCode(params_proto);
  ASSERT_TRUE(!result_pair.first.ok());
  EXPECT_THAT(result_pair.first.message(),
              HasSubstr("Execution failed; Error when invoking the handler. "
                        "Uncaught ReferenceError: config is not defined"));
  EXPECT_TRUE(sandbox_api.Stop().ok());
}

TEST(WorkerSandboxApiTest,
     StartingTheSandboxShouldFailIfNotEnoughMemoryInRlimitForV8) {
  // Since this is limiting the virtual memory space in a machine with swap and
  // no other limitations, this limit needs to be pretty high for V8 to properly
  // start. We set a limit of 100MB which causes a failure in this case.
  WorkerSandboxApi sandbox_api(
      /*require_preload=*/false, /*native_js_function_comms_fd=*/-1,
      /*native_js_function_names=*/{}, /*rpc_method_names=*/{},
      /*server_address=*/"", /*max_worker_virtual_memory_mb=*/100,
      /*js_engine_initial_heap_size_mb=*/0,
      /*js_engine_maximum_heap_size_mb=*/0,
      /*js_engine_max_wasm_memory_number_of_pages=*/0,
      /*sandbox_request_response_shared_buffer_size_mb=*/0,
      /*enable_sandbox_sharing_request_response_with_buffer_only=*/false,
      /*v8_flags=*/{});

  // Initializing the sandbox fail as we're giving a max of 100MB of virtual
  // space address for v8 and the sandbox.
  EXPECT_FALSE(sandbox_api.Init().ok());

  EXPECT_TRUE(sandbox_api.Stop().ok());
}

TEST(WorkerSandboxApiTest, WorkerCanCallHooksThroughSandbox) {
  int fds[2];
  EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds), 0);

  WorkerSandboxApi sandbox_api(
      /*require_preload=*/false, /*native_js_function_comms_fd=*/fds[1],
      /*native_js_function_names=*/{"my_great_func"}, /*rpc_method_names=*/{},
      /*server_address=*/"",
      /*max_worker_virtual_memory_mb=*/0, /*js_engine_initial_heap_size_mb=*/0,
      /*js_engine_maximum_heap_size_mb=*/0,
      /*js_engine_max_wasm_memory_number_of_pages=*/0,
      /*sandbox_request_response_shared_buffer_size_mb=*/0,
      /*enable_sandbox_sharing_request_response_with_buffer_only=*/false,
      /*v8_flags=*/{});

  ASSERT_TRUE(sandbox_api.Init().ok());

  std::thread to_handle_function_call(
      [](int fd) {
        sandbox2::Comms comms(fd);
        RpcWrapper rpc_proto;
        EXPECT_TRUE(comms.RecvProtoBuf(&rpc_proto));

        auto result = "from C++ " + rpc_proto.io_proto().input_string();
        rpc_proto.mutable_io_proto()->set_output_string(result);

        EXPECT_TRUE(comms.SendProtoBuf(rpc_proto));
      },
      fds[0]);

  ASSERT_TRUE(sandbox_api.Run().ok());

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(
      "function cool_func(input) { return my_great_func(input) };");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;
  params_proto.mutable_input_strings()->mutable_inputs()->Add(R"("from JS")");

  std::pair<absl::Status, WorkerSandboxApi::RetryStatus> result_pair =
      sandbox_api.RunCode(params_proto);
  ASSERT_TRUE(result_pair.first.ok());
  EXPECT_EQ(result_pair.second, WorkerSandboxApi::RetryStatus::kDoNotRetry);

  to_handle_function_call.join();

  EXPECT_THAT(params_proto.response(), StrEq(R"("from C++ from JS")"));

  EXPECT_TRUE(sandbox_api.Stop().ok());
}

class WorkerSandboxApiForTests : public WorkerSandboxApi {
 public:
  WorkerSandboxApiForTests(
      bool require_preload, int native_js_function_comms_fd,
      const std::vector<std::string>& native_js_function_names)
      : WorkerSandboxApi(
            require_preload, native_js_function_comms_fd,
            native_js_function_names, /*rpc_method_names=*/{},
            /*server_address=*/"",
            /*max_worker_virtual_memory_mb=*/0,
            /*js_engine_initial_heap_size_mb=*/0,
            /*js_engine_maximum_heap_size_mb=*/0,
            /*js_engine_max_wasm_memory_number_of_pages=*/0,
            /*sandbox_request_response_shared_buffer_size_mb=*/0,
            /*enable_sandbox_sharing_request_response_with_buffer_only=*/false,
            /*v8_flags=*/{}) {}

  ::sapi::Sandbox* GetUnderlyingSandbox() { return worker_sapi_sandbox_.get(); }
};

TEST(WorkerSandboxApiTest, SandboxShouldComeBackUpIfItDies) {
  WorkerSandboxApiForTests sandbox_api(
      /*require_preload=*/false, /*native_js_function_comms_fd=*/-1,
      /*native_js_function_names=*/std::vector<std::string>());

  ASSERT_TRUE(sandbox_api.Init().ok());

  ASSERT_TRUE(sandbox_api.Run().ok());

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(
      R"js(function cool_func() { return "Hi there from sandboxed JS :)" })js");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  int sandbox_pid = sandbox_api.GetUnderlyingSandbox()->pid();
  EXPECT_EQ(kill(sandbox_pid, SIGKILL), 0);
  // Wait for the sandbox to die
  while (sandbox_api.GetUnderlyingSandbox()->is_active()) {
  }

  // We expect a failure since the worker process died
  {
    std::pair<absl::Status, WorkerSandboxApi::RetryStatus> result_pair =
        sandbox_api.RunCode(params_proto);
    EXPECT_FALSE(result_pair.first.ok());
    EXPECT_EQ(result_pair.second, WorkerSandboxApi::RetryStatus::kRetry);
  }

  // Run code again and this time it should work
  std::pair<absl::Status, WorkerSandboxApi::RetryStatus> result_pair =
      sandbox_api.RunCode(params_proto);
  ASSERT_TRUE(result_pair.first.ok());
  EXPECT_EQ(result_pair.second, WorkerSandboxApi::RetryStatus::kDoNotRetry);
  EXPECT_THAT(params_proto.response(),
              StrEq(R"js("Hi there from sandboxed JS :)")js"));

  EXPECT_TRUE(sandbox_api.Stop().ok());
}

TEST(WorkerSandboxApiTest,
     SandboxShouldComeBackUpIfItDiesAndHooksShouldContinueWorking) {
  int fds[2];
  EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds), 0);

  WorkerSandboxApiForTests sandbox_api(
      /*require_preload=*/false, /*native_js_function_comms_fd=*/fds[1],
      /*native_js_function_names=*/{"my_great_func"});

  ASSERT_TRUE(sandbox_api.Init().ok());

  std::thread to_handle_function_call(
      [](int fd) {
        sandbox2::Comms comms(fd);
        RpcWrapper rpc_proto;
        EXPECT_TRUE(comms.RecvProtoBuf(&rpc_proto));

        auto result = absl::StrCat("from C++ hook :) ",
                                   rpc_proto.io_proto().input_string());
        rpc_proto.mutable_io_proto()->set_output_string(result);

        EXPECT_TRUE(comms.SendProtoBuf(rpc_proto));
      },
      fds[0]);

  ASSERT_TRUE(sandbox_api.Run().ok());

  ::worker_api::WorkerParamsProto params_proto;
  // Code calls a hook: "my_great_func"
  params_proto.set_code(
      "function cool_func(input) { return my_great_func(input) };");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;
  params_proto.mutable_input_strings()->mutable_inputs()->Add(R"("from JS")");

  int sandbox_pid = sandbox_api.GetUnderlyingSandbox()->pid();
  EXPECT_EQ(kill(sandbox_pid, SIGKILL), 0);
  // Wait for the sandbox to die
  while (sandbox_api.GetUnderlyingSandbox()->is_active()) {
  }

  // This is expected to fail since we killed the sandbox
  {
    std::pair<absl::Status, WorkerSandboxApi::RetryStatus> result_pair =
        sandbox_api.RunCode(params_proto);
    EXPECT_FALSE(result_pair.first.ok());
    EXPECT_EQ(result_pair.second, WorkerSandboxApi::RetryStatus::kRetry);
  }

  // We run the code again and expect it to work this time around since the
  // sandbox should have been restarted
  std::pair<absl::Status, WorkerSandboxApi::RetryStatus> result_pair =
      sandbox_api.RunCode(params_proto);
  ASSERT_TRUE(result_pair.first.ok());
  EXPECT_EQ(result_pair.second, WorkerSandboxApi::RetryStatus::kDoNotRetry);

  to_handle_function_call.join();

  EXPECT_THAT(params_proto.response(), StrEq(R"("from C++ hook :) from JS")"));

  EXPECT_TRUE(sandbox_api.Stop().ok());
}
}  // namespace google::scp::roma::sandbox::worker_api::test
