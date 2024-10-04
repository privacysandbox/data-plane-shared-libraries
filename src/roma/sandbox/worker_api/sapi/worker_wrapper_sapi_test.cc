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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <signal.h>

#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "sandboxed_api/sandbox2/buffer.h"
#include "sandboxed_api/sandbox2/comms.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/native_function_binding/rpc_wrapper.pb.h"
#include "src/roma/sandbox/worker_api/sapi/utils.h"
#include "src/roma/sandbox/worker_api/sapi/worker_wrapper.h"

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
constexpr size_t kBufferSize = 1 * 1024 * 1024 /* 1Mib */;
std::unique_ptr<sandbox2::Buffer> buffer_ptr_;

static ::worker_api::WorkerInitParamsProto GetDefaultInitParams() {
  // create a sandbox2 buffer
  auto buffer = sandbox2::Buffer::CreateWithSize(kBufferSize);
  EXPECT_TRUE(buffer.ok());
  buffer_ptr_ = std::move(buffer).value();

  ::worker_api::WorkerInitParamsProto init_params;
  init_params.set_require_code_preload_for_execution(false);
  init_params.set_native_js_function_comms_fd(-1);
  init_params.set_server_address("");
  init_params.mutable_native_js_function_names()->Clear();
  init_params.mutable_rpc_method_names()->Clear();
  init_params.set_js_engine_initial_heap_size_mb(0);
  init_params.set_js_engine_maximum_heap_size_mb(0);
  init_params.set_js_engine_max_wasm_memory_number_of_pages(0);
  init_params.set_request_and_response_data_buffer_fd(buffer_ptr_->fd());
  init_params.set_request_and_response_data_buffer_size_bytes(kBufferSize);
  init_params.set_skip_v8_cleanup(true);
  init_params.mutable_v8_flags()->Clear();
  init_params.set_enable_profilers(false);
  return init_params;
}

TEST(WorkerWrapperSapiTest,
     StartingTheSandboxShouldFailIfNotEnoughMemoryInRlimitForV8) {
  auto init_params = GetDefaultInitParams();

  // Since this is limiting the virtual memory space in a machine with swap and
  // no other limitations, this limit needs to be pretty high for V8 to properly
  // start. We set a limit of 100MB which causes a failure in this case.
  WorkerWrapper worker(
      /*enable_sandbox_sharing_request_response_with_buffer_only=*/false,
      /*request_and_response_data_buffer_size_bytes=*/0,
      /*sandbox_data_shared_buffer_ptr=*/buffer_ptr_.get(),
      /*native_js_function_comms_fd=*/-1,
      /*max_worker_virtual_memory_mb=*/100);

  // Initializing the sandbox fail as we're giving a max of 100MB of virtual
  // space address for v8 and the sandbox.
  EXPECT_FALSE(worker.Init(init_params).ok());

  EXPECT_TRUE(worker.Stop().ok());
}

class WorkerWrapperForTests : public WorkerWrapper {
 public:
  explicit WorkerWrapperForTests(int native_js_function_comms_fd)
      : WorkerWrapper(
            /*enable_sandbox_sharing_request_response_with_buffer_only=*/false,
            /*request_and_response_data_buffer_size_bytes=*/0,
            /*sandbox_data_shared_buffer_ptr=*/buffer_ptr_.get(),
            /*native_js_function_comms_fd=*/native_js_function_comms_fd,
            /*max_worker_virtual_memory_mb=*/0) {}

  ::sapi::Sandbox* GetUnderlyingSandbox() { return worker_sapi_sandbox_.get(); }
};

TEST(WorkerWrapperSapiTest, SandboxShouldComeBackUpIfItDies) {
  auto init_params = GetDefaultInitParams();
  WorkerWrapperForTests worker(/*native_js_function_comms_fd=*/-1);

  ASSERT_TRUE(worker.Init(init_params).ok());

  ASSERT_TRUE(worker.Run().ok());

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(
      R"js(function cool_func() { return "Hi there from sandboxed JS :)"
      })js");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  int sandbox_pid = worker.GetUnderlyingSandbox()->pid();
  EXPECT_EQ(kill(sandbox_pid, SIGKILL), 0);
  // Wait for the sandbox to die
  while (worker.GetUnderlyingSandbox()->is_active()) {
  }

  // We expect a failure since the worker process died
  {
    std::pair<absl::Status, RetryStatus> result_pair =
        worker.RunCode(params_proto);
    EXPECT_FALSE(result_pair.first.ok());
    EXPECT_EQ(result_pair.second, RetryStatus::kRetry);
  }

  // Run code again and this time it should work
  std::pair<absl::Status, RetryStatus> result_pair =
      worker.RunCode(params_proto);
  EXPECT_EQ(result_pair.second, RetryStatus::kDoNotRetry);
  ASSERT_TRUE(result_pair.first.ok());
  EXPECT_EQ(result_pair.second, RetryStatus::kDoNotRetry);
  EXPECT_THAT(params_proto.response(),
              StrEq(R"js("Hi there from sandboxed JS :)")js"));

  EXPECT_TRUE(worker.Stop().ok());
}

TEST(WorkerWrapperSapiTest,
     SandboxShouldComeBackUpIfItDiesAndHooksShouldContinueWorking) {
  int fds[2];
  EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds), 0);

  auto init_params = GetDefaultInitParams();
  init_params.set_native_js_function_comms_fd(fds[1]);
  init_params.add_native_js_function_names("my_great_func");

  WorkerWrapperForTests worker(/*native_js_function_comms_fd=*/fds[1]);

  ASSERT_TRUE(worker.Init(init_params).ok());

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

  ASSERT_TRUE(worker.Run().ok());

  ::worker_api::WorkerParamsProto params_proto;
  // Code calls a hook: "my_great_func"
  params_proto.set_code(
      "function cool_func(input) { return my_great_func(input) };");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;
  params_proto.mutable_input_strings()->mutable_inputs()->Add(R"("from JS")");

  int sandbox_pid = worker.GetUnderlyingSandbox()->pid();
  EXPECT_EQ(kill(sandbox_pid, SIGKILL), 0);
  // Wait for the sandbox to die
  while (worker.GetUnderlyingSandbox()->is_active()) {
  }

  // This is expected to fail since we killed the sandbox
  {
    std::pair<absl::Status, RetryStatus> result_pair =
        worker.RunCode(params_proto);
    EXPECT_FALSE(result_pair.first.ok());
    EXPECT_EQ(result_pair.second, RetryStatus::kRetry);
  }

  // We run the code again and expect it to work this time around since the
  // sandbox should have been restarted
  std::pair<absl::Status, RetryStatus> result_pair =
      worker.RunCode(params_proto);
  ASSERT_TRUE(result_pair.first.ok());
  EXPECT_EQ(result_pair.second, RetryStatus::kDoNotRetry);

  to_handle_function_call.join();

  EXPECT_THAT(params_proto.response(), StrEq(R"("from C++ hook :) from JS")"));

  EXPECT_TRUE(worker.Stop().ok());
}

TEST(WorkerWrapperSapiTest, WorkerCanCallHooksThroughSandbox) {
  int fds[2];
  EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds), 0);

  auto init_params = GetDefaultInitParams();
  init_params.set_native_js_function_comms_fd(fds[1]);
  init_params.add_native_js_function_names("my_great_func");

  WorkerWrapperForTests worker(/*native_js_function_comms_fd=*/fds[1]);

  ASSERT_TRUE(worker.Init(init_params).ok());

  std::thread to_handle_function_call(
      [](int fd) {
        sandbox2::Comms comms(fd);
        RpcWrapper rpc_proto;
        EXPECT_TRUE(comms.RecvProtoBuf(&rpc_proto));

        auto result =
            absl::StrCat("from C++ ", rpc_proto.io_proto().input_string());
        rpc_proto.mutable_io_proto()->set_output_string(result);

        EXPECT_TRUE(comms.SendProtoBuf(rpc_proto));
      },
      fds[0]);

  ASSERT_TRUE(worker.Run().ok());

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(
      "function cool_func(input) { return my_great_func(input) };");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;
  params_proto.mutable_input_strings()->mutable_inputs()->Add(R"("from JS")");

  std::pair<absl::Status, RetryStatus> result_pair =
      worker.RunCode(params_proto);
  ASSERT_TRUE(result_pair.first.ok());
  EXPECT_EQ(result_pair.second, RetryStatus::kDoNotRetry);

  to_handle_function_call.join();

  EXPECT_THAT(params_proto.response(), StrEq(R"("from C++ from JS")"));

  EXPECT_TRUE(worker.Stop().ok());
}
}  // namespace google::scp::roma::sandbox::worker_api::test
