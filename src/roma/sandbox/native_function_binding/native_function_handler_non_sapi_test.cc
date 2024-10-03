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

#include "src/roma/sandbox/native_function_binding/native_function_handler_non_sapi.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sys/socket.h>

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "src/roma/metadata_storage/metadata_storage.h"
#include "src/roma/sandbox/native_function_binding/native_function_table.h"
#include "src/roma/sandbox/native_function_binding/rpc_wrapper.pb.h"

using google::scp::roma::metadata_storage::MetadataStorage;
using google::scp::roma::sandbox::native_function_binding::
    NativeFunctionHandlerNonSapi;
using google::scp::roma::sandbox::native_function_binding::NativeFunctionTable;
using ::testing::SizeIs;
using ::testing::StrEq;

namespace google::scp::roma::sandbox::native_function_binding::test {
namespace {
constexpr std::string_view kRequestUuid = "foo";

TEST(NativeFunctionHandlerNonSapiTest, IninRunStop) {
  int fd_pair[2];
  EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_pair), 0);
  std::vector<int> local_fds = {fd_pair[0]};
  std::vector<int> remote_fds = {fd_pair[1]};
  NativeFunctionTable function_table;
  MetadataStorage<google::scp::roma::DefaultMetadata> metadata_storage;
  NativeFunctionHandlerNonSapi handler(&function_table, &metadata_storage,
                                       local_fds, remote_fds);

  handler.Run();
  handler.Stop();
}

static bool g_called_registered_function;

void FunctionToBeCalled(FunctionBindingPayload<>& wrapper) {
  g_called_registered_function = true;
  wrapper.io_proto.set_output_string("I'm an output standalone string");
}

TEST(NativeFunctionHandlerNonSapiTest, ShouldCallFunctionWhenRegistered) {
  int fd_pair[2];
  EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_pair), 0);
  std::vector<int> local_fds = {fd_pair[0]};
  std::vector<int> remote_fds = {fd_pair[1]};
  NativeFunctionTable function_table;
  function_table.Register("cool_function_name", FunctionToBeCalled)
      .IgnoreError();
  MetadataStorage<google::scp::roma::DefaultMetadata> metadata_storage;
  NativeFunctionHandlerNonSapi handler(&function_table, &metadata_storage,
                                       local_fds, remote_fds);
  handler.Run();
  metadata_storage.Add(std::string{kRequestUuid}, {}).IgnoreError();
  g_called_registered_function = false;

  auto remote_fd = remote_fds.at(0);
  proto::RpcWrapper rpc_proto;
  rpc_proto.set_function_name("cool_function_name");
  rpc_proto.set_request_uuid(std::string{kRequestUuid});
  std::string serialized_proto = rpc_proto.SerializeAsString();

  // Send the request over so that it's handled and the registered function
  // can be called
  EXPECT_NE(write(remote_fd, serialized_proto.c_str(), serialized_proto.size()),
            -1);
  // Receive the response
  char buffer[1024] = {0};
  ssize_t bytesRead = read(remote_fd, buffer, sizeof(buffer) - 1);
  EXPECT_NE(bytesRead, -1);
  std::string serialized_response = std::string(buffer, bytesRead);
  rpc_proto.ParseFromString(serialized_response);

  EXPECT_TRUE(g_called_registered_function);
  EXPECT_THAT(rpc_proto.io_proto().output_string(),
              StrEq("I'm an output standalone string"));
  handler.Stop();
}

TEST(NativeFunctionHandlerNonSapiTest,
     ShouldAddErrorsIfFunctionNameIsNotFoundInTable) {
  int fd_pair[2];
  EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_pair), 0);
  std::vector<int> local_fds = {fd_pair[0]};
  std::vector<int> remote_fds = {fd_pair[1]};
  NativeFunctionTable function_table;
  // We don't register any functions with the function table
  MetadataStorage<google::scp::roma::DefaultMetadata> metadata_storage;
  NativeFunctionHandlerNonSapi handler(&function_table, &metadata_storage,
                                       local_fds, remote_fds);
  handler.Run();
  metadata_storage.Add(std::string{kRequestUuid}, {}).IgnoreError();

  g_called_registered_function = false;

  auto remote_fd = remote_fds.at(0);
  proto::RpcWrapper rpc_proto;
  rpc_proto.set_function_name("cool_function_name");
  rpc_proto.set_request_uuid(std::string{kRequestUuid});
  std::string serialized_proto = rpc_proto.SerializeAsString();
  // Send the request over so that it's handled and the registered function
  // can be called
  EXPECT_NE(write(remote_fd, serialized_proto.c_str(), serialized_proto.size()),
            -1);
  // Receive the response
  char buffer[1024] = {0};
  ssize_t bytesRead = read(remote_fd, buffer, sizeof(buffer) - 1);
  EXPECT_NE(bytesRead, -1);
  std::string serialized_response = std::string(buffer, bytesRead);
  rpc_proto.ParseFromString(serialized_response);

  EXPECT_FALSE(g_called_registered_function);
  EXPECT_FALSE(rpc_proto.io_proto().has_input_string() ||
               rpc_proto.io_proto().has_input_list_of_string() ||
               rpc_proto.io_proto().has_input_map_of_string());
  EXPECT_GE(rpc_proto.io_proto().errors().size(), 0);
  EXPECT_THAT(rpc_proto.io_proto().errors(0),
              StrEq("ROMA: Failed to execute the C++ function."));
  handler.Stop();
}

TEST(NativeFunctionHandlerNonSapiTest,
     ShouldAddErrorsIfFunctionNameIsNotInMetadata) {
  int fd_pair[2];
  EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_pair), 0);
  std::vector<int> local_fds = {fd_pair[0]};
  std::vector<int> remote_fds = {fd_pair[1]};
  NativeFunctionTable function_table;
  // We don't register any functions with the function table
  MetadataStorage<google::scp::roma::DefaultMetadata> metadata_storage;
  NativeFunctionHandlerNonSapi handler(&function_table, &metadata_storage,
                                       local_fds, remote_fds);
  handler.Run();

  g_called_registered_function = false;

  auto remote_fd = remote_fds.at(0);
  proto::RpcWrapper rpc_proto;
  // Set request_id to avoid writing empty string
  rpc_proto.set_request_id("foo");
  std::string serialized_proto = rpc_proto.SerializeAsString();
  // Send the request over so that it's handled and the registered function
  // can be called
  EXPECT_NE(write(remote_fd, serialized_proto.c_str(), serialized_proto.size()),
            -1);
  // Receive the response
  char buffer[1024] = {0};
  ssize_t bytesRead = read(remote_fd, buffer, sizeof(buffer) - 1);
  EXPECT_NE(bytesRead, -1);
  std::string serialized_response = std::string(buffer, bytesRead);
  rpc_proto.ParseFromString(serialized_response);

  EXPECT_FALSE(g_called_registered_function);
  EXPECT_FALSE(rpc_proto.io_proto().has_input_string() ||
               rpc_proto.io_proto().has_input_list_of_string() ||
               rpc_proto.io_proto().has_input_map_of_string());
  EXPECT_GE(rpc_proto.io_proto().errors().size(), 0);
  EXPECT_THAT(rpc_proto.io_proto().errors(0),
              StrEq("ROMA: Could not find C++ function by name."));
  handler.Stop();
}

static bool g_called_registered_function_one;
static bool g_called_registered_function_two;

void FunctionOne(FunctionBindingPayload<>& wrapper) {
  g_called_registered_function_one = true;
  wrapper.io_proto.set_output_string("From function one");
}

void FunctionTwo(FunctionBindingPayload<>& wrapper) {
  g_called_registered_function_two = true;
  wrapper.io_proto.set_output_string("From function two");
}

TEST(NativeFunctionHandlerNonSapiTest, ShouldBeAbleToCallMultipleFunctions) {
  int fd_pair[2];
  EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_pair), 0);
  std::vector<int> local_fds = {fd_pair[0]};
  std::vector<int> remote_fds = {fd_pair[1]};
  NativeFunctionTable function_table;
  function_table.Register("cool_function_name_one", FunctionOne).IgnoreError();
  function_table.Register("cool_function_name_two", FunctionTwo).IgnoreError();
  MetadataStorage<google::scp::roma::DefaultMetadata> metadata_storage;
  NativeFunctionHandlerNonSapi handler(&function_table, &metadata_storage,
                                       local_fds, remote_fds);
  handler.Run();
  metadata_storage.Add(absl::StrCat(kRequestUuid, 1), {}).IgnoreError();

  g_called_registered_function_one = false;
  g_called_registered_function_two = false;

  auto remote_fd = remote_fds.at(0);
  proto::RpcWrapper rpc_proto;
  rpc_proto.set_function_name("cool_function_name_one");
  rpc_proto.set_request_uuid(absl::StrCat(kRequestUuid, 1));
  std::string serialized_proto = rpc_proto.SerializeAsString();
  // Send the request over so that it's handled and the registered function
  // can be called
  EXPECT_NE(write(remote_fd, serialized_proto.c_str(), serialized_proto.size()),
            -1);
  // Receive the response
  char buffer[1024] = {0};
  ssize_t bytesRead = read(remote_fd, buffer, sizeof(buffer) - 1);
  EXPECT_NE(bytesRead, -1);
  std::string serialized_response = std::string(buffer, bytesRead);
  rpc_proto.ParseFromString(serialized_response);

  EXPECT_TRUE(g_called_registered_function_one);
  EXPECT_EQ(rpc_proto.io_proto().errors().size(), 0);
  EXPECT_THAT(rpc_proto.io_proto().output_string(), StrEq("From function one"));

  rpc_proto.Clear();
  rpc_proto.set_function_name("cool_function_name_two");
  rpc_proto.set_request_uuid(absl::StrCat(kRequestUuid, 2));
  metadata_storage.Add(absl::StrCat(kRequestUuid, 2), {}).IgnoreError();
  serialized_proto = rpc_proto.SerializeAsString();
  // Send the request over so that it's handled and the registered function
  // can be called
  EXPECT_NE(write(remote_fd, serialized_proto.c_str(), serialized_proto.size()),
            -1);
  // Receive the response
  char buffer2[1024] = {0};
  bytesRead = read(remote_fd, buffer2, sizeof(buffer2) - 1);
  EXPECT_NE(bytesRead, -1);
  serialized_response = std::string(buffer2, bytesRead);
  rpc_proto.ParseFromString(serialized_response);

  EXPECT_TRUE(g_called_registered_function_two);
  EXPECT_EQ(rpc_proto.io_proto().errors().size(), 0);
  EXPECT_THAT(rpc_proto.io_proto().output_string(), StrEq("From function two"));
  handler.Stop();
}
}  // namespace
}  // namespace google::scp::roma::sandbox::native_function_binding::test
