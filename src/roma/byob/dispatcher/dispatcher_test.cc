// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/roma/byob/dispatcher/dispatcher.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/notification.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/byob/dispatcher/dispatcher.pb.h"
#include "src/roma/byob/host/callback.pb.h"
#include "src/roma/byob/udf/sample.pb.h"
#include "src/roma/config/function_binding_object_v2.h"

namespace privacy_sandbox::server_common::byob {
namespace {
using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using ::google::protobuf::util::SerializeDelimitedToFileDescriptor;
using ::google::scp::roma::FunctionBindingPayload;
using ::privacy_sandbox::server_common::byob::FUNCTION_CALLBACK;
using ::privacy_sandbox::server_common::byob::FUNCTION_HELLO_WORLD;
using ::privacy_sandbox::server_common::byob::FUNCTION_PRIME_SIEVE;
using ::privacy_sandbox::server_common::byob::FUNCTION_TEN_CALLBACK_INVOCATIONS;
using ::testing::Contains;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

TEST(DispatcherTest, ShutdownPreInit) { Dispatcher dispatcher; }

void BindAndListenOnPath(int fd, std::string_view path) {
  sockaddr_un sa;
  ::memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  ::strncpy(sa.sun_path, path.data(), sizeof(sa.sun_path));
  ASSERT_EQ(0, ::bind(fd, reinterpret_cast<sockaddr*>(&sa), SUN_LEN(&sa)));
  ASSERT_EQ(0, ::listen(fd, /*backlog=*/0));
}

void ConnectToPath(int fd, std::string_view path) {
  sockaddr_un sa;
  ::memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  ::strncpy(sa.sun_path, path.data(), sizeof(sa.sun_path));
  ASSERT_EQ(0, ::connect(fd, reinterpret_cast<sockaddr*>(&sa), SUN_LEN(&sa)));
}

TEST(DispatcherTest, ShutdownWorkerThenDispatcher) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  absl::Cleanup cleanup = [] { EXPECT_EQ(::unlink("abcd.sock"), 0); };
  std::thread worker([] {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);
    ConnectToPath(fd, "abcd.sock");
    EXPECT_EQ(::close(fd), 0);
  });
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd).ok());
  worker.join();
}

TEST(DispatcherTest, ShutdownDispatcherThenWorker) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  absl::Cleanup cleanup = [] { EXPECT_EQ(::unlink("abcd.sock"), 0); };
  absl::Notification done;
  std::thread worker([&done] {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);
    ConnectToPath(fd, "abcd.sock");
    done.WaitForNotification();
    EXPECT_EQ(::close(fd), 0);
  });
  {
    Dispatcher dispatcher;
    ASSERT_TRUE(dispatcher.Init(fd).ok());
  }
  done.Notify();
  worker.join();
}

TEST(DispatcherTest, LoadGoesToWorker) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  absl::Cleanup cleanup = [] { EXPECT_EQ(::unlink("abcd.sock"), 0); };
  std::thread worker([] {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);
    ConnectToPath(fd, "abcd.sock");
    LoadRequest request;
    FileInputStream input(fd);
    ASSERT_TRUE(ParseDelimitedFromZeroCopyStream(&request, &input, nullptr));
    EXPECT_THAT(request.binary_path(), StrEq("dummy_code_path"));
    ASSERT_EQ(request.code_token().size(), 36);
    EXPECT_EQ(request.n_workers(), 7);
    for (int i = 0; i < request.n_workers(); ++i) {
      const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
      ASSERT_NE(fd, -1);
      ConnectToPath(fd, "abcd.sock");
      EXPECT_EQ(::write(fd, request.code_token().c_str(), 36), 36);
    }
    EXPECT_EQ(::close(fd), 0);
  });
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd).ok());
  dispatcher.LoadBinary("dummy_code_path", /*n_workers=*/7);
  worker.join();
}

TEST(DispatcherTest, LoadAndExecute) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  absl::Cleanup cleanup = [] { EXPECT_EQ(::unlink("abcd.sock"), 0); };
  std::thread worker([] {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);
    ConnectToPath(fd, "abcd.sock");

    // Process load request.
    LoadRequest request;
    {
      FileInputStream input(fd);
      ASSERT_TRUE(ParseDelimitedFromZeroCopyStream(&request, &input, nullptr));
    }
    EXPECT_THAT(request.binary_path(), StrEq("dummy_code_path"));
    ASSERT_EQ(request.code_token().size(), 36);
    EXPECT_EQ(request.n_workers(), 1);

    // Process execution request.
    const int connection_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(connection_fd, -1);
    ConnectToPath(connection_fd, "abcd.sock");
    EXPECT_EQ(::write(connection_fd, request.code_token().c_str(), 36), 36);
    {
      // Read UDF input.
      google::protobuf::Any any;
      FileInputStream input(connection_fd);
      ASSERT_TRUE(ParseDelimitedFromZeroCopyStream(&any, &input, nullptr));
      SampleRequest request;
      ASSERT_TRUE(any.UnpackTo(&request));
      EXPECT_EQ(request.function(), FUNCTION_HELLO_WORLD);
    }
    {
      // Write UDF output.
      SampleResponse response;
      response.set_greeting("dummy greeting");
      google::protobuf::Any any;
      ASSERT_TRUE(any.PackFrom(std::move(response)));
      ASSERT_TRUE(SerializeDelimitedToFileDescriptor(any, connection_fd));
    }
    EXPECT_EQ(::close(connection_fd), 0);
    EXPECT_EQ(::close(fd), 0);
  });
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd).ok());
  const std::string code_token =
      dispatcher.LoadBinary("dummy_code_path", /*n_workers=*/1);
  {
    google::protobuf::Any serialized_request;
    {
      SampleRequest request;
      request.set_function(FUNCTION_HELLO_WORLD);
      ASSERT_TRUE(serialized_request.PackFrom(request));
    }
    absl::flat_hash_map<std::string,
                        std::function<void(FunctionBindingPayload<int>&)>>
        function_table;
    absl::StatusOr<std::string> bin_response;
    absl::Notification done;
    dispatcher.ExecuteBinary(code_token, serialized_request, /*metadata=*/0,
                             function_table, [&](auto response) {
                               bin_response = std::move(response);
                               done.Notify();
                             });
    done.WaitForNotification();
    ASSERT_TRUE(bin_response.ok());
    EXPECT_TRUE(SampleResponse{}.ParseFromString(*bin_response));
  }
  worker.join();
}

TEST(DispatcherTest, LoadAndExecuteWithCallbacks) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  absl::Cleanup cleanup = [] { EXPECT_EQ(::unlink("abcd.sock"), 0); };
  std::thread worker([] {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);
    ConnectToPath(fd, "abcd.sock");

    // Process load request.
    LoadRequest request;
    {
      FileInputStream input(fd);
      ASSERT_TRUE(ParseDelimitedFromZeroCopyStream(&request, &input, nullptr));
    }
    EXPECT_THAT(request.binary_path(), StrEq("dummy_code_path"));
    ASSERT_EQ(request.code_token().size(), 36);
    EXPECT_EQ(request.n_workers(), 1);

    // Process execution request.
    const int connection_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(connection_fd, -1);
    ConnectToPath(connection_fd, "abcd.sock");
    EXPECT_EQ(::write(connection_fd, request.code_token().c_str(), 36), 36);

    // Read UDF input.
    FileInputStream input(connection_fd);
    {
      google::protobuf::Any any;
      ASSERT_TRUE(ParseDelimitedFromZeroCopyStream(&any, &input, nullptr));
      SampleRequest request;
      ASSERT_TRUE(any.UnpackTo(&request));
      EXPECT_EQ(request.function(), FUNCTION_PRIME_SIEVE);
    }

    // Initiate host callbacks from UDF.
    for (const auto& id : {"A", "B"}) {
      Callback callback;
      callback.set_id(id);
      callback.set_function_name("example_function");
      google::protobuf::Any any;
      ASSERT_TRUE(any.PackFrom(std::move(callback)));
      ASSERT_TRUE(SerializeDelimitedToFileDescriptor(any, connection_fd));
    }

    // Accept callback responses.
    std::vector<std::string> response_ids(2);
    for (std::string& id : response_ids) {
      Callback callback;
      ASSERT_TRUE(ParseDelimitedFromZeroCopyStream(&callback, &input, nullptr));
      EXPECT_THAT(callback.function_name(), StrEq("example_function"));
      ASSERT_TRUE(callback.has_id());
      id = std::move(*callback.mutable_id());
    }
    EXPECT_THAT(response_ids, UnorderedElementsAre("A", "B"));
    {
      // Write UDF output.
      SampleResponse response;
      response.set_greeting("dummy greeting");
      google::protobuf::Any any;
      ASSERT_TRUE(any.PackFrom(std::move(response)));
      ASSERT_TRUE(SerializeDelimitedToFileDescriptor(any, connection_fd));
    }
    EXPECT_EQ(::close(connection_fd), 0);
    EXPECT_EQ(::close(fd), 0);
  });
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd).ok());
  const std::string code_token =
      dispatcher.LoadBinary("dummy_code_path", /*n_workers=*/1);
  {
    google::protobuf::Any serialized_request;
    {
      SampleRequest request;
      request.set_function(FUNCTION_PRIME_SIEVE);
      ASSERT_TRUE(serialized_request.PackFrom(request));
    }
    absl::Mutex mu;
    int count = 0;  // Guarded by mu.
    absl::flat_hash_map<
        std::string, std::function<void(FunctionBindingPayload<std::string>&)>>
        function_table = {{"example_function", [&](auto& payload) {
                             EXPECT_THAT(payload.metadata, StrEq("dummy_data"));
                             absl::MutexLock lock(&mu);
                             ++count;
                           }}};
    absl::StatusOr<std::string> bin_response;
    absl::Notification done;
    dispatcher.ExecuteBinary(code_token, serialized_request,
                             /*metadata=*/std::string{"dummy_data"},
                             function_table, [&](auto response) {
                               bin_response = std::move(response);
                               done.Notify();
                             });
    done.WaitForNotification();
    ASSERT_TRUE(bin_response.ok());
    EXPECT_TRUE(SampleResponse{}.ParseFromString(*bin_response));
    EXPECT_EQ(count, 2);
  }
  worker.join();
}

TEST(DispatcherTest, LoadAndExecuteWithCallbacksAndMetadata) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  absl::Cleanup cleanup = [] { EXPECT_EQ(::unlink("abcd.sock"), 0); };
  std::thread worker([] {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);
    ConnectToPath(fd, "abcd.sock");

    // Process load request.
    std::string code_token;
    {
      LoadRequest request;
      FileInputStream input(fd);
      ASSERT_TRUE(ParseDelimitedFromZeroCopyStream(&request, &input, nullptr));
      EXPECT_THAT(request.binary_path(), StrEq("dummy_code_path"));
      ASSERT_EQ(request.code_token().size(), 36);
      code_token = std::move(*request.mutable_code_token());
      EXPECT_EQ(request.n_workers(), 1);
    }

    // Process execution requests.
    for (int i = 0; i < 100; ++i) {
      const int connection_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
      ASSERT_NE(connection_fd, -1);
      ConnectToPath(connection_fd, "abcd.sock");
      EXPECT_EQ(::write(connection_fd, code_token.c_str(), 36), 36);

      // Read UDF input.
      FileInputStream input(connection_fd);
      {
        google::protobuf::Any any;
        ASSERT_TRUE(ParseDelimitedFromZeroCopyStream(&any, &input, nullptr));
        EXPECT_TRUE(any.Is<SampleRequest>());
      }
      {
        // Initiate host callback from UDF.
        Callback callback;
        callback.set_function_name("example_function");
        google::protobuf::Any any;
        ASSERT_TRUE(any.PackFrom(std::move(callback)));
        ASSERT_TRUE(SerializeDelimitedToFileDescriptor(any, connection_fd));
      }
      {
        // Accept callback response.
        Callback callback;
        ASSERT_TRUE(
            ParseDelimitedFromZeroCopyStream(&callback, &input, nullptr));
      }
      {
        // Write UDF output.
        SampleResponse response;
        google::protobuf::Any any;
        ASSERT_TRUE(any.PackFrom(std::move(response)));
        ASSERT_TRUE(SerializeDelimitedToFileDescriptor(any, connection_fd));
      }
      EXPECT_EQ(::close(connection_fd), 0);
    }
    EXPECT_EQ(::close(fd), 0);
  });
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd).ok());
  const std::string code_token =
      dispatcher.LoadBinary("dummy_code_path", /*n_workers=*/1);
  google::protobuf::Any serialized_request;
  ASSERT_TRUE(serialized_request.PackFrom(SampleRequest{}));
  absl::Mutex mu;
  absl::flat_hash_set<int> metadatas;  // Guarded by mu.
  metadatas.reserve(100);
  absl::flat_hash_map<std::string,
                      std::function<void(FunctionBindingPayload<int>&)>>
      function_table = {{"example_function", [&](auto& payload) {
                           absl::MutexLock lock(&mu);
                           metadatas.insert(payload.metadata);
                         }}};
  absl::BlockingCounter counter(100);
  for (int i = 0; i < 100; ++i) {
    dispatcher.ExecuteBinary(
        code_token, serialized_request, /*metadata=*/i, function_table,
        [&counter](auto response) { counter.DecrementCount(); });
  }
  counter.Wait();
  for (int i = 0; i < 100; ++i) {
    EXPECT_THAT(metadatas, Contains(i));
  }
  worker.join();
}

}  // namespace
}  // namespace privacy_sandbox::server_common::byob
