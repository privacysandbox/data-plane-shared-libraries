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
#include "absl/synchronization/notification.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/byob/dispatcher/dispatcher.pb.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/util/execution_token.h"

namespace privacy_sandbox::server_common::byob {
namespace {
using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using ::google::protobuf::util::SerializeDelimitedToFileDescriptor;
using ::google::scp::roma::ExecutionToken;
using ::google::scp::roma::FunctionBindingPayload;
using ::privacy_sandbox::roma_byob::example::FUNCTION_HELLO_WORLD;
using ::privacy_sandbox::roma_byob::example::FUNCTION_PRIME_SIEVE;
using ::privacy_sandbox::roma_byob::example::SampleRequest;
using ::privacy_sandbox::roma_byob::example::SampleResponse;
using ::testing::Contains;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

TEST(DispatcherTest, ShutdownPreInit) { Dispatcher dispatcher; }

void BindAndListenOnPath(int fd, std::string_view path) {
  ::sockaddr_un sa = {
      .sun_family = AF_UNIX,
  };
  path.copy(sa.sun_path, sizeof(sa.sun_path));
  ASSERT_EQ(0, ::bind(fd, reinterpret_cast<::sockaddr*>(&sa), SUN_LEN(&sa)));
  ASSERT_EQ(0, ::listen(fd, /*backlog=*/0));
}

void ConnectToPath(int fd, std::string_view path, bool unlink_path = true) {
  ::sockaddr_un sa = {
      .sun_family = AF_UNIX,
  };
  path.copy(sa.sun_path, sizeof(sa.sun_path));
  ASSERT_EQ(0, ::connect(fd, reinterpret_cast<::sockaddr*>(&sa), SUN_LEN(&sa)));
  if (unlink_path) {
    EXPECT_EQ(::unlink(path.data()), 0);
  }
}

TEST(DispatcherTest, ShutdownWorkerThenDispatcher) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  std::thread worker([] {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);
    ConnectToPath(fd, "abcd.sock");
    EXPECT_EQ(::close(fd), 0);
  });
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd, /*logdir=*/"").ok());
  worker.join();
}

TEST(DispatcherTest, ShutdownDispatcherThenWorker) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
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
    ASSERT_TRUE(dispatcher.Init(fd, /*logdir=*/"").ok());
  }
  done.Notify();
  worker.join();
}

TEST(DispatcherTest, LoadErrorsForEmptyBinaryPath) {
  Dispatcher dispatcher;
  EXPECT_FALSE(dispatcher.LoadBinary("", /*num_workers=*/1).ok());
}

// TODO: b/371538589 - Ensure non-file paths are handled appropriately.
TEST(DispatcherTest, DISABLED_LoadErrorsForRootPath) {
  Dispatcher dispatcher;
  EXPECT_FALSE(dispatcher.LoadBinary("/", /*num_workers=*/1).ok());
}

TEST(DispatcherTest, LoadErrorsForUnknownBinaryPath) {
  Dispatcher dispatcher;
  EXPECT_FALSE(
      dispatcher.LoadBinary("/asdflkj/ytrewq", /*num_workers=*/1).ok());
}

TEST(DispatcherTest, LoadErrorsWhenNWorkersNonPositive) {
  Dispatcher dispatcher;
  EXPECT_FALSE(dispatcher
                   .LoadBinary("src/roma/byob/sample_udf/new_udf",
                               /*num_workers=*/0)
                   .ok());
}

TEST(DispatcherTest, LoadErrorsWhenFileDoesntExist) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  absl::Notification done;
  std::thread worker([&done] {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);
    ConnectToPath(fd, "abcd.sock");
    done.WaitForNotification();
    EXPECT_EQ(::close(fd), 0);
  });
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd, /*logdir=*/"").ok());
  const absl::StatusOr<std::string> code_token = dispatcher.LoadBinary(
      "src/roma/byob/sample_udf/fake_udf", /*num_workers=*/7);
  EXPECT_FALSE(code_token.ok());
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
    ConnectToPath(fd, "abcd.sock", /*unlink_path=*/false);
    DispatcherRequest request;
    {
      FileInputStream input(fd);
      ASSERT_TRUE(ParseDelimitedFromZeroCopyStream(&request, &input, nullptr));
    }
    ASSERT_TRUE(request.has_load_binary());
    ASSERT_EQ(request.load_binary().code_token().size(), kNumTokenBytes);
    EXPECT_EQ(request.load_binary().num_workers(), 7);
    EXPECT_EQ(::close(fd), 0);
  });
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd, /*logdir=*/"").ok());
  EXPECT_TRUE(dispatcher
                  .LoadBinary("src/roma/byob/sample_udf/new_udf",
                              /*num_workers=*/7)
                  .ok());
  worker.join();
}

TEST(DispatcherTest, LoadAndDeleteGoToWorker) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  absl::Cleanup cleanup = [] { EXPECT_EQ(::unlink("abcd.sock"), 0); };
  std::thread worker([] {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);
    ConnectToPath(fd, "abcd.sock", /*unlink_path=*/false);
    FileInputStream input(fd);
    DispatcherRequest load_request;
    ASSERT_TRUE(
        ParseDelimitedFromZeroCopyStream(&load_request, &input, nullptr));
    ASSERT_TRUE(load_request.has_load_binary());
    ASSERT_EQ(load_request.load_binary().code_token().size(), kNumTokenBytes);
    EXPECT_EQ(load_request.load_binary().num_workers(), 3);
    DispatcherRequest delete_request;
    ASSERT_TRUE(
        ParseDelimitedFromZeroCopyStream(&delete_request, &input, nullptr));
    ASSERT_TRUE(delete_request.has_delete_binary());
    EXPECT_THAT(delete_request.delete_binary().code_token(),
                StrEq(load_request.load_binary().code_token()));
    EXPECT_EQ(::close(fd), 0);
  });
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd, /*logdir=*/"").ok());
  const absl::StatusOr<std::string> code_token = dispatcher.LoadBinary(
      "src/roma/byob/sample_udf/new_udf", /*n_workers=*/3);
  ASSERT_TRUE(code_token.ok());
  dispatcher.Delete(*code_token);
  {
    SampleRequest bin_request;
    ASSERT_FALSE(dispatcher
                     .ProcessRequest<SampleResponse>(
                         *code_token, bin_request,
                         [](auto /*response*/, auto /*logs*/) {})
                     .ok());
  }
  worker.join();
}

TEST(DispatcherTest, LoadAndExecute) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  std::thread worker([] {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);
    ConnectToPath(fd, "abcd.sock", /*unlink_path=*/false);

    // Process load request.
    DispatcherRequest request;
    {
      FileInputStream input(fd);
      ASSERT_TRUE(ParseDelimitedFromZeroCopyStream(&request, &input, nullptr));
    }
    ASSERT_TRUE(request.has_load_binary());
    ASSERT_EQ(request.load_binary().code_token().size(), kNumTokenBytes);
    EXPECT_EQ(request.load_binary().num_workers(), 1);

    // Process execution request.
    const int connection_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(connection_fd, -1);
    ConnectToPath(connection_fd, "abcd.sock");
    EXPECT_EQ(::write(connection_fd, request.load_binary().code_token().c_str(),
                      kNumTokenBytes),
              kNumTokenBytes);
    {
      const std::string execution_token(kNumTokenBytes, 'a');
      EXPECT_EQ(::write(connection_fd, execution_token.c_str(), kNumTokenBytes),
                kNumTokenBytes);
    }
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
  ASSERT_TRUE(dispatcher.Init(fd, /*logdir=*/"").ok());
  const absl::StatusOr<std::string> code_token = dispatcher.LoadBinary(
      "src/roma/byob/sample_udf/new_udf", /*num_workers=*/1);
  ASSERT_TRUE(code_token.ok());
  {
    SampleRequest bin_request;
    bin_request.set_function(FUNCTION_HELLO_WORLD);
    absl::StatusOr<SampleResponse> bin_response;
    absl::Notification done;
    ASSERT_TRUE(
        dispatcher
            .ProcessRequest<SampleResponse>(
                *code_token, bin_request,
                [&](auto response, absl::StatusOr<std::string_view> logs) {
                  bin_response = std::move(response);
                  done.Notify();
                })
            .ok());
    done.WaitForNotification();
    EXPECT_TRUE(bin_response.ok());
  }
  worker.join();
}

TEST(DispatcherTest, LoadAndCloseBeforeExecute) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, /*protocol=*/0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  std::thread worker([] {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, /*protocol=*/0);
    ASSERT_NE(fd, -1);
    ConnectToPath(fd, "abcd.sock", /*unlink_path=*/false);

    // Process load request.
    DispatcherRequest request;
    {
      FileInputStream input(fd);
      ASSERT_TRUE(ParseDelimitedFromZeroCopyStream(&request, &input, nullptr));
    }
    ASSERT_TRUE(request.has_load_binary());
    ASSERT_EQ(request.load_binary().code_token().size(), kNumTokenBytes);
    EXPECT_EQ(request.load_binary().num_workers(), 1);

    // Process execution request.
    const int connection_fd = ::socket(AF_UNIX, SOCK_STREAM, /*protocol=*/0);
    ASSERT_NE(connection_fd, -1);
    ConnectToPath(connection_fd, "abcd.sock");
    EXPECT_EQ(::write(connection_fd, request.load_binary().code_token().c_str(),
                      /*count=*/kNumTokenBytes),
              kNumTokenBytes);
    {
      const std::string execution_token(kNumTokenBytes, 'a');
      EXPECT_EQ(::write(connection_fd, execution_token.c_str(), kNumTokenBytes),
                kNumTokenBytes);
    }
    EXPECT_EQ(::close(connection_fd), 0);
    EXPECT_EQ(::close(fd), 0);
  });
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd, /*logdir=*/"").ok());
  const absl::StatusOr<std::string> code_token = dispatcher.LoadBinary(
      "src/roma/byob/sample_udf/new_udf", /*num_workers=*/1);
  ASSERT_TRUE(code_token.ok());
  worker.join();
  SampleRequest bin_request;
  bin_request.set_function(FUNCTION_HELLO_WORLD);
  absl::Notification done;
  ASSERT_TRUE(
      dispatcher
          .ProcessRequest<SampleResponse>(
              *code_token, bin_request,
              [&](auto response, absl::StatusOr<std::string_view> logs) {
                done.Notify();
              })
          .ok());
  done.WaitForNotification();
}

TEST(DispatcherTest, LoadAndExecuteThenCancel) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  absl::Cleanup cleanup = [] { EXPECT_EQ(::unlink("abcd.sock"), 0); };
  std::thread worker([] {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);
    ConnectToPath(fd, "abcd.sock", /*unlink_path=*/false);

    // Process load request.
    FileInputStream input(fd);
    DispatcherRequest request;
    ASSERT_TRUE(ParseDelimitedFromZeroCopyStream(&request, &input, nullptr));
    ASSERT_TRUE(request.has_load_binary());
    ASSERT_EQ(request.load_binary().code_token().size(), kNumTokenBytes);
    EXPECT_EQ(request.load_binary().num_workers(), 1);

    // Process execution request.
    const int connection_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(connection_fd, -1);
    ConnectToPath(connection_fd, "abcd.sock", /*unlink_path=*/false);
    EXPECT_EQ(::write(connection_fd, request.load_binary().code_token().c_str(),
                      kNumTokenBytes),
              kNumTokenBytes);
    const std::string execution_token(kNumTokenBytes, 'a');
    EXPECT_EQ(::write(connection_fd, execution_token.c_str(), kNumTokenBytes),
              kNumTokenBytes);
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
      // Read cancellation request.
      DispatcherRequest request;
      ASSERT_TRUE(ParseDelimitedFromZeroCopyStream(&request, &input, nullptr));
      ASSERT_TRUE(request.has_cancel());
      EXPECT_THAT(request.cancel().execution_token(), StrEq(execution_token));
    }
    EXPECT_EQ(::close(connection_fd), 0);
    EXPECT_EQ(::close(fd), 0);
  });
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd, /*logdir=*/"").ok());
  const absl::StatusOr<std::string> code_token = dispatcher.LoadBinary(
      "src/roma/byob/sample_udf/new_udf", /*n_workers=*/1);
  ASSERT_TRUE(code_token.ok());
  {
    SampleRequest bin_request;
    bin_request.set_function(FUNCTION_HELLO_WORLD);
    absl::Notification done;
    absl::StatusOr<ExecutionToken> execution_token =
        dispatcher.ProcessRequest<SampleResponse>(
            *code_token, bin_request,
            [&done](auto response, absl::StatusOr<std::string_view> /*logs*/) {
              done.Notify();
            });
    ASSERT_TRUE(execution_token.ok());
    dispatcher.Cancel(*std::move(execution_token));
    done.WaitForNotification();
  }
  worker.join();
}

}  // namespace
}  // namespace privacy_sandbox::server_common::byob
