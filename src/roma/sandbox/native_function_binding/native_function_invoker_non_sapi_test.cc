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

#include <sys/socket.h>

#include <string>
#include <thread>

#include "src/roma/sandbox/native_function_binding/native_function_invoker.h"
#include "src/roma/sandbox/native_function_binding/rpc_wrapper.pb.h"

using google::scp::roma::sandbox::native_function_binding::
    NativeFunctionInvoker;
using ::testing::StrEq;

namespace google::scp::roma::sandbox::native_function_binding::test {
namespace {
TEST(NativeFunctionInvokerNonSapiTest, ShouldReturnFailureOnInvokeIfBadFd) {
  NativeFunctionInvoker invoker(-1);

  proto::RpcWrapper rpc_proto;
  EXPECT_FALSE(invoker.Invoke(rpc_proto).ok());
}

TEST(NativeFunctionInvokerNonSapiTest, ShouldMakeCallOnFd) {
  int fd_pair[2];
  EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_pair), 0);

  NativeFunctionInvoker invoker(fd_pair[0]);

  std::thread to_handle_message([fd = fd_pair[1]]() {
    char buffer[1024] = {0};
    ssize_t bytesRead = read(fd, buffer, sizeof(buffer) - 1);
    EXPECT_NE(bytesRead, -1);

    std::string serialized_proto = std::string(buffer, bytesRead);
    proto::RpcWrapper rpc_proto;
    EXPECT_TRUE(rpc_proto.ParseFromString(serialized_proto));
    rpc_proto.mutable_io_proto()->set_output_string("Some string");

    std::string serialized_wrapper = rpc_proto.SerializeAsString();
    ssize_t sentBytes =
        write(fd, serialized_wrapper.c_str(), serialized_wrapper.size());
    EXPECT_NE(sentBytes, -1);
  });

  proto::RpcWrapper rpc_proto;
  // Set field on rpc_proto, as read() won't unblock for 0-byte message
  rpc_proto.set_request_id("foo");
  ASSERT_TRUE(invoker.Invoke(rpc_proto).ok());

  to_handle_message.join();

  EXPECT_THAT(rpc_proto.io_proto().output_string(), StrEq("Some string"));
}
}  // namespace
}  // namespace google::scp::roma::sandbox::native_function_binding::test
