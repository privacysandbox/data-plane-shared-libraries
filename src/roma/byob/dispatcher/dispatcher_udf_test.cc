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

#include <functional>
#include <string>
#include <string_view>
#include <thread>

#include "absl/base/log_severity.h"
#include "absl/cleanup/cleanup.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "google/protobuf/any.pb.h"
#include "src/roma/byob/dispatcher/dispatcher.h"
#include "src/roma/byob/interface/metrics.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"
#include "src/util/execution_token.h"

namespace privacy_sandbox::server_common::byob {
namespace {
using ::google::scp::roma::ExecutionToken;
using ::privacy_sandbox::roma_byob::example::FUNCTION_HELLO_WORLD;
using ::privacy_sandbox::roma_byob::example::FUNCTION_PRIME_SIEVE;
using ::privacy_sandbox::roma_byob::example::SampleRequest;
using ::privacy_sandbox::roma_byob::example::SampleResponse;
using ::testing::Contains;
using ::testing::StrEq;

absl::Duration kTimeout = absl::Seconds(1);

TEST(DispatcherUdfTest, LoadAndExecuteCppSampleUdfUnspecified) {
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--control_socket_name=xyzw.sock",
        "--udf_socket_name=abcd.sock",
        "--binary_dir=src/roma/byob/sample_udf",
        nullptr,
    };
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::kill(pid, SIGTERM), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
    ASSERT_EQ(::unlink("abcd.sock"), 0);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher
                  .Init(/*control_socket_name=*/"xyzw.sock",
                        /*udf_socket_name=*/"abcd.sock", /*log_dir=*/"",
                        /*binary_dir=*/"src/roma/byob/sample_udf/")
                  .ok());
  const absl::StatusOr<std::string> code_token =
      dispatcher.LoadBinary("src/roma/byob/sample_udf/sample_udf",
                            /*num_workers=*/10);
  ASSERT_TRUE(code_token.ok()) << code_token.status();
  SampleRequest bin_request;
  for (int i = 0; i < 100; ++i) {
    absl::StatusOr<SampleResponse> bin_response;
    absl::Notification done;
    ASSERT_TRUE(
        dispatcher
            .ProcessRequest<SampleResponse>(
                *code_token, bin_request, kTimeout,
                [&bin_response, &done](
                    auto response, absl::StatusOr<std::string_view> /*logs*/,
                    ProcessRequestMetrics /*metrics*/) {
                  bin_response = std::move(response);
                  done.Notify();
                })
            .ok());
    ASSERT_TRUE(done.WaitForNotificationWithTimeout(kTimeout));
    EXPECT_FALSE(bin_response.ok());
  }
}

TEST(DispatcherUdfTest, LoadAndExecuteCppSampleUdfHelloWorld) {
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--control_socket_name=xyzw.sock",
        "--udf_socket_name=abcd.sock",
        "--binary_dir=src/roma/byob/sample_udf",
        nullptr,
    };
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::kill(pid, SIGTERM), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
    ASSERT_EQ(::unlink("abcd.sock"), 0);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher
                  .Init(/*control_socket_name=*/"xyzw.sock",
                        /*udf_socket_name=*/"abcd.sock", /*logdir=*/"",
                        /*binary_dir=*/"src/roma/byob/sample_udf/")
                  .ok());
  const absl::StatusOr<std::string> code_token =
      dispatcher.LoadBinary("src/roma/byob/sample_udf/sample_udf",
                            /*num_workers=*/10);
  ASSERT_TRUE(code_token.ok()) << code_token.status();
  SampleRequest bin_request;
  bin_request.set_function(FUNCTION_HELLO_WORLD);
  for (int i = 0; i < 100; ++i) {
    absl::StatusOr<SampleResponse> bin_response;
    absl::Notification done;
    ASSERT_TRUE(
        dispatcher
            .ProcessRequest<SampleResponse>(
                *code_token, bin_request, kTimeout,
                [&bin_response, &done](
                    auto response, absl::StatusOr<std::string_view> /*logs*/,
                    ProcessRequestMetrics /*metrics*/) {
                  bin_response = std::move(response);
                  done.Notify();
                })
            .ok());
    ASSERT_TRUE(done.WaitForNotificationWithTimeout(kTimeout));
    ASSERT_TRUE(bin_response.ok()) << bin_response.status();
    EXPECT_THAT(bin_response->greeting(), StrEq("Hello, world!"));
  }
}

TEST(DispatcherUdfTest, LoadAndExecuteCppSampleUdfPrimeSieve) {
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--control_socket_name=xyzw.sock",
        "--udf_socket_name=abcd.sock",
        "--binary_dir=src/roma/byob/sample_udf",
        nullptr,
    };
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::kill(pid, SIGTERM), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
    ASSERT_EQ(::unlink("abcd.sock"), 0);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher
                  .Init(/*control_socket_name=*/"xyzw.sock",
                        /*udf_socket_name=*/"abcd.sock", /*logdir=*/"",
                        /*binary_dir=*/"src/roma/byob/sample_udf")
                  .ok());
  const absl::StatusOr<std::string> code_token =
      dispatcher.LoadBinary("src/roma/byob/sample_udf/sample_udf",
                            /*num_workers=*/10);
  ASSERT_TRUE(code_token.ok()) << code_token.status();
  SampleRequest bin_request;
  bin_request.set_function(FUNCTION_PRIME_SIEVE);
  for (int i = 0; i < 100; ++i) {
    absl::StatusOr<SampleResponse> bin_response;
    absl::Notification done;
    ASSERT_TRUE(
        dispatcher
            .ProcessRequest<SampleResponse>(
                *code_token, bin_request, kTimeout,
                [&bin_response, &done](
                    auto response, absl::StatusOr<std::string_view> /*logs*/,
                    ProcessRequestMetrics /*metrics*/) {
                  bin_response = std::move(response);
                  done.Notify();
                })
            .ok());
    ASSERT_TRUE(done.WaitForNotificationWithTimeout(kTimeout));
    EXPECT_TRUE(bin_response.ok()) << bin_response.status();
  }
}

TEST(DispatcherUdfTest, LoadAndExecuteNewUdf) {
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--control_socket_name=xyzw.sock",
        "--udf_socket_name=abcd.sock",
        "--binary_dir=src/roma/byob/sample_udf",
        nullptr,
    };
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::kill(pid, SIGTERM), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
    ASSERT_EQ(::unlink("abcd.sock"), 0);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher
                  .Init(/*control_socket_name=*/"xyzw.sock",
                        /*udf_socket_name=*/"abcd.sock", /*logdir=*/"",
                        /*binary_dir=*/"src/roma/byob/sample_udf")
                  .ok());
  const absl::StatusOr<std::string> code_token =
      dispatcher.LoadBinary("src/roma/byob/sample_udf/new_udf",
                            /*num_workers=*/10);
  ASSERT_TRUE(code_token.ok()) << code_token.status();
  SampleRequest bin_request;
  for (int i = 0; i < 100; ++i) {
    absl::StatusOr<SampleResponse> bin_response;
    absl::Notification done;
    ASSERT_TRUE(
        dispatcher
            .ProcessRequest<SampleResponse>(
                *code_token, bin_request, kTimeout,
                [&bin_response, &done](
                    auto response, absl::StatusOr<std::string_view> /*logs*/,
                    ProcessRequestMetrics /*metrics*/) {
                  bin_response = std::move(response);
                  done.Notify();
                })
            .ok());
    ASSERT_TRUE(done.WaitForNotificationWithTimeout(kTimeout));
    ASSERT_TRUE(bin_response.ok()) << bin_response.status();
    EXPECT_THAT(bin_response->greeting(), StrEq("I am a new UDF!"));
  }
}

TEST(DispatcherUdfTest, LoadAndExecuteAbortUdf) {
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--control_socket_name=xyzw.sock",
        "--udf_socket_name=abcd.sock",
        "--binary_dir=src/roma/byob/sample_udf",
        nullptr,
    };
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::kill(pid, SIGTERM), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
    ASSERT_EQ(::unlink("abcd.sock"), 0);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher
                  .Init(/*control_socket_name=*/"xyzw.sock",
                        /*udf_socket_name=*/"abcd.sock", /*logdir=*/"",
                        /*binary_dir=*/"src/roma/byob/sample_udf")
                  .ok());
  const absl::StatusOr<std::string> code_token =
      dispatcher.LoadBinary("src/roma/byob/sample_udf/abort_late_udf",
                            /*num_workers=*/10);
  ASSERT_TRUE(code_token.ok()) << code_token.status();
  SampleRequest bin_request;
  for (int i = 0; i < 100; ++i) {
    absl::StatusOr<SampleResponse> bin_response;
    absl::Notification done;
    ASSERT_TRUE(
        dispatcher
            .ProcessRequest<SampleResponse>(
                *code_token, bin_request, kTimeout,
                [&bin_response, &done](
                    auto response, absl::StatusOr<std::string_view> /*logs*/,
                    ProcessRequestMetrics /*metrics*/) {
                  bin_response = std::move(response);
                  done.Notify();
                })
            .ok());
    ASSERT_TRUE(done.WaitForNotificationWithTimeout(kTimeout));
    ASSERT_TRUE(bin_response.ok()) << bin_response.status();
    EXPECT_THAT(bin_response->greeting(), StrEq("I am a crashing UDF!"));
  }
}

TEST(DispatcherUdfTest, LoadAndExecuteNonzeroReturnUdf) {
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--control_socket_name=xyzw.sock",
        "--udf_socket_name=abcd.sock",
        "--binary_dir=src/roma/byob/sample_udf",
        nullptr,
    };
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::kill(pid, SIGTERM), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
    ASSERT_EQ(::unlink("abcd.sock"), 0);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher
                  .Init(/*control_socket_name=*/"xyzw.sock",
                        /*udf_socket_name=*/"abcd.sock", /*logdir=*/"",
                        /*binary_dir=*/"src/roma/byob/sample_udf")
                  .ok());
  const absl::StatusOr<std::string> code_token =
      dispatcher.LoadBinary("src/roma/byob/sample_udf/nonzero_return_udf",
                            /*num_workers=*/10);
  ASSERT_TRUE(code_token.ok()) << code_token.status();
  SampleRequest bin_request;
  for (int i = 0; i < 100; ++i) {
    absl::StatusOr<SampleResponse> bin_response;
    absl::Notification done;
    ASSERT_TRUE(
        dispatcher
            .ProcessRequest<SampleResponse>(
                *code_token, bin_request, kTimeout,
                [&bin_response, &done](
                    auto response, absl::StatusOr<std::string_view> /*logs*/,
                    ProcessRequestMetrics /*metrics*/) {
                  bin_response = std::move(response);
                  done.Notify();
                })
            .ok());
    ASSERT_TRUE(done.WaitForNotificationWithTimeout(kTimeout));
    ASSERT_TRUE(bin_response.ok()) << bin_response.status();
    EXPECT_THAT(bin_response->greeting(), StrEq("I return a non-zero status!"));
  }
}

TEST(DispatcherUdfTest, LoadExecuteAndDeletePauseUdfThenLoadAndExecuteNewUdf) {
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--control_socket_name=xyzw.sock",
        "--udf_socket_name=abcd.sock",
        "--binary_dir=src/roma/byob/sample_udf",
        nullptr,
    };
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::kill(pid, SIGTERM), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
    ASSERT_EQ(::unlink("abcd.sock"), 0);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher
                  .Init(/*control_socket_name=*/"xyzw.sock",
                        /*udf_socket_name=*/"abcd.sock", /*logdir=*/"",
                        /*binary_dir=*/"src/roma/byob/sample_udf")
                  .ok());
  SampleRequest bin_request;
  {
    const absl::StatusOr<std::string> code_token =
        dispatcher.LoadBinary("src/roma/byob/sample_udf/pause_udf",
                              /*n_workers=*/2);
    ASSERT_TRUE(code_token.ok()) << code_token.status();
    absl::Notification done;
    ASSERT_TRUE(dispatcher
                    .ProcessRequest<SampleResponse>(
                        *code_token, bin_request, kTimeout,
                        [&done](auto /*response*/, auto /*logs*/,
                                auto /*metrics*/) { done.Notify(); })
                    .ok());
    EXPECT_FALSE(done.WaitForNotificationWithTimeout(absl::Seconds(1)));
    dispatcher.Delete(*code_token);
    ASSERT_TRUE(done.WaitForNotificationWithTimeout(kTimeout));
  }
  {
    const absl::StatusOr<std::string> code_token =
        dispatcher.LoadBinary("src/roma/byob/sample_udf/new_udf",
                              /*n_workers=*/2);
    ASSERT_TRUE(code_token.ok()) << code_token.status();
    absl::Notification done;
    absl::StatusOr<SampleResponse> bin_response;
    ASSERT_TRUE(
        dispatcher
            .ProcessRequest<SampleResponse>(
                *code_token, bin_request, kTimeout,
                [&bin_response, &done](
                    auto response, absl::StatusOr<std::string_view> /*logs*/,
                    ProcessRequestMetrics /*metrics*/) {
                  bin_response = std::move(response);
                  done.Notify();
                })
            .ok());
    ASSERT_TRUE(done.WaitForNotificationWithTimeout(kTimeout));
    ASSERT_TRUE(bin_response.ok()) << bin_response.status();
    EXPECT_THAT(bin_response->greeting(), StrEq("I am a new UDF!"));
  }
}

TEST(DispatcherUdfTest, LoadExecuteAndCancelPauseUdf) {
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--control_socket_name=xyzw.sock",
        "--udf_socket_name=abcd.sock",
        "--binary_dir=src/roma/byob/sample_udf",
        nullptr,
    };
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::kill(pid, SIGTERM), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
    ASSERT_EQ(::unlink("abcd.sock"), 0);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher
                  .Init(/*control_socket_name=*/"xyzw.sock",
                        /*udf_socket_name=*/"abcd.sock", /*logdir=*/"",
                        /*binary_dir=*/"src/roma/byob/sample_udf")
                  .ok());
  const absl::StatusOr<std::string> code_token =
      dispatcher.LoadBinary("src/roma/byob/sample_udf/pause_udf",
                            /*n_workers=*/2);
  ASSERT_TRUE(code_token.ok()) << code_token.status();
  SampleRequest bin_request;
  absl::Notification done;
  absl::StatusOr<ExecutionToken> execution_token =
      dispatcher.ProcessRequest<SampleResponse>(
          *code_token, bin_request, kTimeout,
          [&done](auto /*response*/, absl::StatusOr<std::string_view> /*logs*/,
                  ProcessRequestMetrics metrics) {
            EXPECT_GT(metrics.response_time, absl::Seconds(1));
            done.Notify();
          });
  ASSERT_TRUE(execution_token.ok()) << execution_token.status();
  EXPECT_FALSE(done.WaitForNotificationWithTimeout(absl::Seconds(2)));
  dispatcher.Cancel(*std::move(execution_token));
  ASSERT_TRUE(done.WaitForNotificationWithTimeout(kTimeout));
}

TEST(DispatcherUdfTest, LoadAndExecuteGoSampleUdfUnspecified) {
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--control_socket_name=xyzw.sock",
        "--udf_socket_name=abcd.sock",
        "--binary_dir=src/roma/byob/sample_udf",
        nullptr,
    };
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed: ";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::kill(pid, SIGTERM), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
    ASSERT_EQ(::unlink("abcd.sock"), 0);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher
                  .Init(/*control_socket_name=*/"xyzw.sock",
                        /*udf_socket_name=*/"abcd.sock", /*logdir=*/"",
                        /*binary_dir=*/"src/roma/byob/sample_udf")
                  .ok());
  const absl::StatusOr<std::string> code_token = dispatcher.LoadBinary(
      "src/roma/byob/sample_udf/sample_go_udf_/sample_go_udf",
      /*num_workers=*/10);
  ASSERT_TRUE(code_token.ok()) << code_token.status();
  SampleRequest bin_request;
  for (int i = 0; i < 100; ++i) {
    absl::StatusOr<SampleResponse> bin_response;
    absl::Notification done;
    ASSERT_TRUE(
        dispatcher
            .ProcessRequest<SampleResponse>(
                *code_token, bin_request, kTimeout,
                [&bin_response, &done](
                    auto response, absl::StatusOr<std::string_view> /*logs*/,
                    ProcessRequestMetrics /*metrics*/) {
                  bin_response = std::move(response);
                  done.Notify();
                })
            .ok());
    ASSERT_TRUE(done.WaitForNotificationWithTimeout(kTimeout));
    EXPECT_FALSE(bin_response.ok());
  }
}

TEST(DispatcherUdfTest, LoadAndExecuteGoSampleUdfHelloWorld) {
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--control_socket_name=xyzw.sock",
        "--udf_socket_name=abcd.sock",
        "--binary_dir=src/roma/byob/sample_udf",
        nullptr,
    };
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed: ";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::kill(pid, SIGTERM), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
    ASSERT_EQ(::unlink("abcd.sock"), 0);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher
                  .Init(/*control_socket_name=*/"xyzw.sock",
                        /*udf_socket_name=*/"abcd.sock", /*logdir=*/"",
                        /*binary_dir=*/"src/roma/byob/sample_udf")
                  .ok());
  const absl::StatusOr<std::string> code_token = dispatcher.LoadBinary(
      "src/roma/byob/sample_udf/sample_go_udf_/sample_go_udf",
      /*num_workers=*/10);
  ASSERT_TRUE(code_token.ok()) << code_token.status();
  SampleRequest bin_request;
  bin_request.set_function(FUNCTION_HELLO_WORLD);
  for (int i = 0; i < 100; ++i) {
    absl::StatusOr<SampleResponse> bin_response;
    absl::Notification done;
    ASSERT_TRUE(
        dispatcher
            .ProcessRequest<SampleResponse>(
                *code_token, bin_request, kTimeout,
                [&bin_response, &done](
                    auto response, absl::StatusOr<std::string_view> /*logs*/,
                    ProcessRequestMetrics /*metrics*/) {
                  bin_response = std::move(response);
                  done.Notify();
                })
            .ok());
    ASSERT_TRUE(done.WaitForNotificationWithTimeout(kTimeout));
    ASSERT_TRUE(bin_response.ok()) << bin_response.status();
    EXPECT_THAT(bin_response->greeting(), StrEq("Hello, world from Go!"));
  }
}

TEST(DispatcherUdfTest, LoadAndExecuteGoSampleUdfPrimeSieve) {
  const int pid = ::vfork();
  ASSERT_NE(pid, -1);
  if (pid == 0) {
    const char* argv[] = {
        "src/roma/byob/dispatcher/run_workers_without_sandbox",
        "--control_socket_name=xyzw.sock",
        "--udf_socket_name=abcd.sock",
        "--binary_dir=src/roma/byob/sample_udf",
        nullptr,
    };
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve() failed: ";
  }
  absl::Cleanup cleanup = [pid] {
    ASSERT_EQ(::kill(pid, SIGTERM), 0);
    ASSERT_NE(::waitpid(pid, nullptr, /*options=*/0), -1);
    ASSERT_EQ(::unlink("abcd.sock"), 0);
  };
  Dispatcher dispatcher;
  ASSERT_TRUE(dispatcher
                  .Init(/*control_socket_name=*/"xyzw.sock",
                        /*udf_socket_name=*/"abcd.sock", /*logdir=*/"",
                        /*binary_dir=*/"src/roma/byob/sample_udf")
                  .ok());
  const absl::StatusOr<std::string> code_token = dispatcher.LoadBinary(
      "src/roma/byob/sample_udf/sample_go_udf_/sample_go_udf",
      /*num_workers=*/10);
  ASSERT_TRUE(code_token.ok()) << code_token.status();
  SampleRequest bin_request;
  bin_request.set_function(FUNCTION_PRIME_SIEVE);
  for (int i = 0; i < 100; ++i) {
    absl::StatusOr<SampleResponse> bin_response;
    absl::Notification done;
    ASSERT_TRUE(
        dispatcher
            .ProcessRequest<SampleResponse>(
                *code_token, bin_request, kTimeout,
                [&bin_response, &done](
                    auto response, absl::StatusOr<std::string_view> /*logs*/,
                    ProcessRequestMetrics /*metrics*/) {
                  bin_response = std::move(response);
                  done.Notify();
                })
            .ok());
    ASSERT_TRUE(done.WaitForNotificationWithTimeout(kTimeout));
    EXPECT_TRUE(bin_response.ok()) << bin_response.status();
  }
}
}  // namespace
}  // namespace privacy_sandbox::server_common::byob

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverity::kWarning);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
