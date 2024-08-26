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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <functional>
#include <string>
#include <thread>

#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "google/protobuf/any.pb.h"
#include "src/roma/byob/dispatcher/dispatcher.h"
#include "src/roma/byob/udf/sample.pb.h"
#include "src/roma/config/function_binding_object_v2.h"

namespace privacy_sandbox::server_common::byob {
namespace {
using ::google::scp::roma::FunctionBindingPayload;
using ::privacy_sandbox::server_common::byob::FUNCTION_CALLBACK;
using ::privacy_sandbox::server_common::byob::FUNCTION_HELLO_WORLD;
using ::privacy_sandbox::server_common::byob::FUNCTION_PRIME_SIEVE;
using ::privacy_sandbox::server_common::byob::FUNCTION_TEN_CALLBACK_INVOCATIONS;
using ::testing::Contains;
using ::testing::StrEq;

void BindAndListenOnPath(int fd, std::string_view path) {
  sockaddr_un sa;
  ::memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  ::strncpy(sa.sun_path, path.data(), sizeof(sa.sun_path));
  ASSERT_EQ(0, ::bind(fd, reinterpret_cast<sockaddr*>(&sa), SUN_LEN(&sa)));
  ASSERT_EQ(0, ::listen(fd, /*backlog=*/0));
}

TEST(DispatcherUdfTest, LoadAndExecuteCppSampleUdfUnspecified) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--socket_name=abcd.sock", nullptr};
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::unlink("abcd.sock"), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd).ok());
  const std::string code_token =
      dispatcher.LoadBinary("src/roma/byob/udf/sample_udf",
                            /*n_workers=*/10);
  google::protobuf::Any serialized_request;
  ASSERT_TRUE(serialized_request.PackFrom(SampleRequest{}));
  absl::flat_hash_map<std::string,
                      std::function<void(FunctionBindingPayload<int>&)>>
      function_table;
  absl::BlockingCounter counter(100);
  for (int i = 0; i < 100; ++i) {
    dispatcher.ExecuteBinary(
        code_token, serialized_request, /*metadata=*/i, function_table,
        [&counter](auto response) {
          ASSERT_TRUE(response.ok());
          SampleResponse bin_response;
          EXPECT_TRUE(bin_response.ParseFromString(*response));
          counter.DecrementCount();
        });
  }
  counter.Wait();
}

TEST(DispatcherUdfTest, LoadAndExecuteCppSampleUdfHelloWorld) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--socket_name=abcd.sock", nullptr};
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::unlink("abcd.sock"), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd).ok());
  const std::string code_token =
      dispatcher.LoadBinary("src/roma/byob/udf/sample_udf",
                            /*n_workers=*/10);
  google::protobuf::Any serialized_request;
  {
    SampleRequest request;
    request.set_function(FUNCTION_HELLO_WORLD);
    ASSERT_TRUE(serialized_request.PackFrom(request));
  }
  absl::flat_hash_map<std::string,
                      std::function<void(FunctionBindingPayload<int>&)>>
      function_table;
  absl::BlockingCounter counter(100);
  for (int i = 0; i < 100; ++i) {
    dispatcher.ExecuteBinary(
        code_token, serialized_request, /*metadata=*/i, function_table,
        [&counter](auto response) {
          ASSERT_TRUE(response.ok());
          SampleResponse bin_response;
          ASSERT_TRUE(bin_response.ParseFromString(*response));
          EXPECT_THAT(bin_response.greeting(), StrEq("Hello, world!"));
          counter.DecrementCount();
        });
  }
  counter.Wait();
}

TEST(DispatcherUdfTest, LoadAndExecuteCppSampleUdfPrimeSieve) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--socket_name=abcd.sock", nullptr};
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::unlink("abcd.sock"), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd).ok());
  const std::string code_token =
      dispatcher.LoadBinary("src/roma/byob/udf/sample_udf",
                            /*n_workers=*/10);
  google::protobuf::Any serialized_request;
  {
    SampleRequest request;
    request.set_function(FUNCTION_PRIME_SIEVE);
    ASSERT_TRUE(serialized_request.PackFrom(request));
  }
  absl::flat_hash_map<std::string,
                      std::function<void(FunctionBindingPayload<int>&)>>
      function_table;
  absl::BlockingCounter counter(100);
  for (int i = 0; i < 100; ++i) {
    dispatcher.ExecuteBinary(
        code_token, serialized_request, /*metadata=*/i, function_table,
        [&counter](auto response) {
          ASSERT_TRUE(response.ok());
          SampleResponse bin_response;
          EXPECT_TRUE(bin_response.ParseFromString(*response));
          counter.DecrementCount();
        });
  }
  counter.Wait();
}

TEST(DispatcherUdfTest, LoadAndExecuteCppSampleUdfCallback) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--socket_name=abcd.sock", nullptr};
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::unlink("abcd.sock"), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd).ok());
  const std::string code_token =
      dispatcher.LoadBinary("src/roma/byob/udf/sample_udf",
                            /*n_workers=*/10);
  google::protobuf::Any serialized_request;
  {
    SampleRequest request;
    request.set_function(FUNCTION_CALLBACK);
    ASSERT_TRUE(serialized_request.PackFrom(request));
  }
  absl::flat_hash_map<std::string,
                      std::function<void(FunctionBindingPayload<int>&)>>
      function_table = {{"example", [&](auto& payload) {}}};
  absl::BlockingCounter counter(100);
  for (int i = 0; i < 100; ++i) {
    dispatcher.ExecuteBinary(
        code_token, serialized_request, /*metadata=*/i, function_table,
        [&counter](auto response) {
          ASSERT_TRUE(response.ok());
          SampleResponse bin_response;
          EXPECT_TRUE(bin_response.ParseFromString(*response));
          counter.DecrementCount();
        });
  }
  counter.Wait();
}

TEST(DispatcherUdfTest, LoadAndExecuteCppSampleUdfTenCallbackInvocations) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--socket_name=abcd.sock", nullptr};
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::unlink("abcd.sock"), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd).ok());
  const std::string code_token =
      dispatcher.LoadBinary("src/roma/byob/udf/sample_udf",
                            /*n_workers=*/10);
  google::protobuf::Any serialized_request;
  {
    SampleRequest request;
    request.set_function(FUNCTION_TEN_CALLBACK_INVOCATIONS);
    ASSERT_TRUE(serialized_request.PackFrom(request));
  }
  absl::flat_hash_map<std::string,
                      std::function<void(FunctionBindingPayload<int>&)>>
      function_table = {{"example", [&](auto& payload) {}}};
  absl::BlockingCounter counter(100);
  for (int i = 0; i < 100; ++i) {
    dispatcher.ExecuteBinary(
        code_token, serialized_request, /*metadata=*/i, function_table,
        [&counter](auto response) {
          ASSERT_TRUE(response.ok());
          SampleResponse bin_response;
          EXPECT_TRUE(bin_response.ParseFromString(*response));
          counter.DecrementCount();
        });
  }
  counter.Wait();
}

TEST(DispatcherUdfTest, LoadAndExecuteNewUdf) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--socket_name=abcd.sock", nullptr};
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::unlink("abcd.sock"), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd).ok());
  const std::string code_token =
      dispatcher.LoadBinary("src/roma/byob/udf/new_udf",
                            /*n_workers=*/10);
  google::protobuf::Any serialized_request;
  ASSERT_TRUE(serialized_request.PackFrom(SampleRequest{}));
  absl::flat_hash_map<std::string,
                      std::function<void(FunctionBindingPayload<int>&)>>
      function_table;
  absl::BlockingCounter counter(100);
  for (int i = 0; i < 100; ++i) {
    dispatcher.ExecuteBinary(
        code_token, serialized_request, /*metadata=*/i, function_table,
        [&counter](auto response) {
          ASSERT_TRUE(response.ok());
          SampleResponse bin_response;
          ASSERT_TRUE(bin_response.ParseFromString(*response));
          EXPECT_THAT(bin_response.greeting(), StrEq("I am a new UDF!"));
          counter.DecrementCount();
        });
  }
  counter.Wait();
}

TEST(DispatcherUdfTest, LoadAndExecuteGoSampleUdfUnspecified) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--socket_name=abcd.sock", nullptr};
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed: ";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::unlink("abcd.sock"), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd).ok());
  const std::string code_token =
      dispatcher.LoadBinary("src/roma/byob/udf/sample_go_udf_/sample_go_udf",
                            /*n_workers=*/10);
  google::protobuf::Any serialized_request;
  ASSERT_TRUE(serialized_request.PackFrom(SampleRequest{}));
  absl::flat_hash_map<std::string,
                      std::function<void(FunctionBindingPayload<int>&)>>
      function_table;
  absl::BlockingCounter counter(100);
  for (int i = 0; i < 100; ++i) {
    dispatcher.ExecuteBinary(
        code_token, serialized_request, /*metadata=*/i, function_table,
        [&counter](auto response) {
          ASSERT_TRUE(response.ok());
          SampleResponse bin_response;
          EXPECT_TRUE(bin_response.ParseFromString(*response));
          counter.DecrementCount();
        });
  }
  counter.Wait();
}

TEST(DispatcherUdfTest, LoadAndExecuteGoSampleUdfHelloWorld) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--socket_name=abcd.sock", nullptr};
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed: ";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::unlink("abcd.sock"), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd).ok());
  const std::string code_token =
      dispatcher.LoadBinary("src/roma/byob/udf/sample_go_udf_/sample_go_udf",
                            /*n_workers=*/10);
  google::protobuf::Any serialized_request;
  {
    SampleRequest request;
    request.set_function(FUNCTION_HELLO_WORLD);
    ASSERT_TRUE(serialized_request.PackFrom(request));
  }
  absl::flat_hash_map<std::string,
                      std::function<void(FunctionBindingPayload<int>&)>>
      function_table;
  absl::BlockingCounter counter(100);
  for (int i = 0; i < 100; ++i) {
    dispatcher.ExecuteBinary(
        code_token, serialized_request, /*metadata=*/i, function_table,
        [&counter](auto response) {
          ASSERT_TRUE(response.ok());
          SampleResponse bin_response;
          ASSERT_TRUE(bin_response.ParseFromString(*response));
          EXPECT_THAT(bin_response.greeting(), StrEq("Hello, world from Go!"));
          counter.DecrementCount();
        });
  }
  counter.Wait();
}

TEST(DispatcherUdfTest, LoadAndExecuteGoSampleUdfPrimeSieve) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--socket_name=abcd.sock", nullptr};
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed: ";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::unlink("abcd.sock"), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd).ok());
  const std::string code_token =
      dispatcher.LoadBinary("src/roma/byob/udf/sample_go_udf_/sample_go_udf",
                            /*n_workers=*/10);
  google::protobuf::Any serialized_request;
  {
    SampleRequest request;
    request.set_function(FUNCTION_PRIME_SIEVE);
    ASSERT_TRUE(serialized_request.PackFrom(request));
  }
  absl::flat_hash_map<std::string,
                      std::function<void(FunctionBindingPayload<int>&)>>
      function_table;
  absl::BlockingCounter counter(100);
  for (int i = 0; i < 100; ++i) {
    dispatcher.ExecuteBinary(
        code_token, serialized_request, /*metadata=*/i, function_table,
        [&counter](auto response) {
          ASSERT_TRUE(response.ok());
          SampleResponse bin_response;
          EXPECT_TRUE(bin_response.ParseFromString(*response));
          counter.DecrementCount();
        });
  }
  counter.Wait();
}

TEST(DispatcherUdfTest, LoadAndExecuteGoSampleUdfCallback) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--socket_name=abcd.sock", nullptr};
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed: ";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::unlink("abcd.sock"), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd).ok());
  const std::string code_token =
      dispatcher.LoadBinary("src/roma/byob/udf/sample_go_udf_/sample_go_udf",
                            /*n_workers=*/10);
  google::protobuf::Any serialized_request;
  {
    SampleRequest request;
    request.set_function(FUNCTION_CALLBACK);
    ASSERT_TRUE(serialized_request.PackFrom(request));
  }
  absl::flat_hash_map<std::string,
                      std::function<void(FunctionBindingPayload<int>&)>>
      function_table = {{"example", [&](auto& payload) {}}};
  absl::BlockingCounter counter(100);
  for (int i = 0; i < 100; ++i) {
    dispatcher.ExecuteBinary(
        code_token, serialized_request, /*metadata=*/i, function_table,
        [&counter](auto response) {
          ASSERT_TRUE(response.ok());
          SampleResponse bin_response;
          EXPECT_TRUE(bin_response.ParseFromString(*response));
          counter.DecrementCount();
        });
  }
  counter.Wait();
}

TEST(DispatcherUdfTest, LoadAndExecuteGoSampleUdfTenCallbackInvocations) {
  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(fd, -1);
  BindAndListenOnPath(fd, "abcd.sock");
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--socket_name=abcd.sock", nullptr};
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed: ";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::unlink("abcd.sock"), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher.Init(fd).ok());
  const std::string code_token =
      dispatcher.LoadBinary("src/roma/byob/udf/sample_go_udf_/sample_go_udf",
                            /*n_workers=*/10);
  google::protobuf::Any serialized_request;
  {
    SampleRequest request;
    request.set_function(FUNCTION_TEN_CALLBACK_INVOCATIONS);
    ASSERT_TRUE(serialized_request.PackFrom(request));
  }
  absl::flat_hash_map<std::string,
                      std::function<void(FunctionBindingPayload<int>&)>>
      function_table = {{"example", [&](auto& payload) {}}};
  absl::BlockingCounter counter(100);
  for (int i = 0; i < 100; ++i) {
    dispatcher.ExecuteBinary(
        code_token, serialized_request, /*metadata=*/i, function_table,
        [&counter](auto response) {
          ASSERT_TRUE(response.ok());
          SampleResponse bin_response;
          EXPECT_TRUE(bin_response.ParseFromString(*response));
          counter.DecrementCount();
        });
  }
  counter.Wait();
}
}  // namespace
}  // namespace privacy_sandbox::server_common::byob
