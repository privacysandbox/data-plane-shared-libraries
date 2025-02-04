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

#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>

#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "src/roma/byob/sample_udf/sample_callback.pb.h"
#include "src/roma/byob/sample_udf/sample_roma_byob_app_service.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"
#include "src/roma/byob/utility/udf_blob.h"
#include "src/roma/byob/utility/utils.h"
#include "src/roma/config/function_binding_object_v2.h"

namespace privacy_sandbox::server_common::byob::test {

namespace {
using ::privacy_sandbox::roma_byob::example::ByobSampleService;
using ::privacy_sandbox::roma_byob::example::FUNCTION_HELLO_WORLD;
using ::privacy_sandbox::roma_byob::example::FUNCTION_PRIME_SIEVE;
using ::privacy_sandbox::roma_byob::example::FunctionType;
using ::privacy_sandbox::roma_byob::example::SampleRequest;
using ::privacy_sandbox::roma_byob::example::SampleResponse;
using ::privacy_sandbox::server_common::byob::HasClonePermissionsByobWorker;
using ::privacy_sandbox::server_common::byob::Mode;
using ::testing::TestWithParam;

const std::filesystem::path kUdfPath = "/udf";
const std::filesystem::path kGoLangBinaryFilename = "sample_go_udf";
const std::filesystem::path kJavaBinaryFilename = "sample_java_native_udf";
const std::filesystem::path kCPlusPlusBinaryFilename = "sample_udf";
const std::filesystem::path kCPlusPlusCapBinaryFilename = "cap_udf";
const std::filesystem::path kCPlusPlusSocketFinderBinaryFilename =
    "socket_finder_udf";
const std::filesystem::path kCPlusPlusFileSystemAddFilename =
    "filesystem_add_udf";
const std::filesystem::path kCPlusPlusFileSystemDeleteFilename =
    "filesystem_delete_udf";
const std::filesystem::path kCPlusPlusFileSystemEditFilename =
    "filesystem_edit_udf";
const std::filesystem::path kCPlusPlusSyscallFilterBinaryFilename =
    "syscall_filter_udf";
const std::filesystem::path kCPlusPlusNewBinaryFilename = "new_udf";
const std::filesystem::path kCPlusPlusLogBinaryFilename = "log_udf";
const std::filesystem::path kCPlusPlusPauseBinaryFilename = "pause_udf";
constexpr std::string_view kFirstUdfOutput = "Hello, world!";
constexpr std::string_view kNewUdfOutput = "I am a new UDF!";
constexpr std::string_view kGoBinaryOutput = "Hello, world from Go!";
constexpr std::string_view kJavaBinaryOutput = "Hello, world from Java!";
constexpr std::string_view kLogUdfOutput = "I am a UDF that logs.";

SampleResponse SendRequestAndGetResponse(
    ByobSampleService<>& roma_service, std::string_view code_token,
    FunctionType func_type = FUNCTION_HELLO_WORLD) {
  // Data we are sending to the server.
  SampleRequest bin_request;
  bin_request.set_function(func_type);
  absl::StatusOr<std::unique_ptr<SampleResponse>> response;

  absl::Notification notif;
  CHECK_OK(roma_service.Sample(notif, std::move(bin_request), response,
                               /*metadata=*/{}, code_token));
  CHECK(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  CHECK_OK(response);
  return *std::move((*response).get());
}

absl::StatusOr<std::string> GetContentsOfFile(std::filesystem::path filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrCat("Failed to open file: ", filename.c_str()));
  }
  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

  file.close();
  return content;
}

std::string LoadCode(ByobSampleService<>& roma_service,
                     std::filesystem::path file_path,
                     bool enable_log_egress = false, int num_workers = 20) {
  absl::StatusOr<std::string> code_id;
  if (!enable_log_egress) {
    code_id = roma_service.Register(file_path, num_workers);
  } else {
    code_id = roma_service.RegisterForLogging(file_path, num_workers);
  }
  CHECK_OK(code_id);
  return *std::move(code_id);
}

std::string LoadCodeFromCodeToken(ByobSampleService<>& roma_service,
                                  std::string no_log_code_token,
                                  int num_workers = 20) {
  absl::StatusOr<std::string> code_id = roma_service.RegisterForLogging(
      no_log_code_token, /*num_workers=*/num_workers);
  CHECK_OK(code_id);
  return *std::move(code_id);
}

ByobSampleService<> GetRomaService(
    ::privacy_sandbox::server_common::byob::Config<> config, Mode mode) {
  absl::StatusOr<ByobSampleService<>> sample_interface =
      ByobSampleService<>::Create(std::move(config), mode);
  CHECK_OK(sample_interface);
  return std::move(*sample_interface);
}

std::pair<SampleResponse, std::string> GetResponseAndLogs(
    ByobSampleService<>& roma_service, std::string_view code_token) {
  absl::Notification exec_notif;
  absl::StatusOr<SampleResponse> bin_response;
  std::string logs_acquired;
  auto callback = [&exec_notif, &bin_response, &logs_acquired](
                      absl::StatusOr<SampleResponse> resp,
                      absl::StatusOr<std::string_view> logs) {
    bin_response = std::move(resp);
    CHECK_OK(logs);
    // Making a copy -- try not to IRL.
    logs_acquired = *logs;
    exec_notif.Notify();
  };

  SampleRequest bin_request;
  CHECK_OK(roma_service.Sample(callback, std::move(bin_request),
                               /*metadata=*/{}, code_token));
  CHECK(exec_notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  CHECK_OK(bin_response);
  return {*bin_response, logs_acquired};
}

std::pair<SampleResponse, absl::Status> GetResponseAndLogStatus(
    ByobSampleService<>& roma_service, std::string_view code_token) {
  absl::Notification exec_notif;
  absl::StatusOr<SampleResponse> bin_response;
  absl::Status log_status;
  auto callback = [&exec_notif, &bin_response, &log_status](
                      absl::StatusOr<SampleResponse> resp,
                      absl::StatusOr<std::string_view> logs) {
    bin_response = std::move(resp);
    log_status = std::move(logs.status());
    exec_notif.Notify();
  };

  SampleRequest bin_request;
  CHECK_OK(roma_service.Sample(callback, std::move(bin_request),
                               /*metadata=*/{}, code_token));
  CHECK(exec_notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  CHECK_OK(bin_response);
  return {*bin_response, log_status};
}

struct RomaByobTestParam {
  Mode mode;
  bool enable_seccomp_filter;
};
using RomaByobTest = TestWithParam<RomaByobTestParam>;

TEST_P(RomaByobTest, NoSocketFile) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.enable_seccomp_filter = param.enable_seccomp_filter}, param.mode);

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusSocketFinderBinaryFilename,
               /*enable_log_egress=*/true, /*num_workers=*/1);

  EXPECT_THAT(SendRequestAndGetResponse(roma_service, code_token).greeting(),
              ::testing::StrEq("Success."));
}

TEST_P(RomaByobTest, NoFileSystemCreateEgression) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.enable_seccomp_filter = param.enable_seccomp_filter}, param.mode);

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusFileSystemAddFilename,
               /*enable_log_egress=*/true, /*num_workers=*/1);

  EXPECT_THAT(SendRequestAndGetResponse(roma_service, code_token).greeting(),
              ::testing::StrEq("Success."));
}

TEST_P(RomaByobTest, NoFileSystemDeleteEgression) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.enable_seccomp_filter = param.enable_seccomp_filter}, param.mode);

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusFileSystemDeleteFilename,
               /*enable_log_egress=*/true, /*num_workers=*/1);

  EXPECT_THAT(SendRequestAndGetResponse(roma_service, code_token).greeting(),
              ::testing::StrEq("Success."));
}

TEST_P(RomaByobTest, NoFileSystemEditEgression) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.enable_seccomp_filter = param.enable_seccomp_filter}, param.mode);

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusFileSystemEditFilename,
               /*enable_log_egress=*/true, /*num_workers=*/1);

  EXPECT_THAT(SendRequestAndGetResponse(roma_service, code_token).greeting(),
              ::testing::StrEq("Success."));
}

TEST_P(RomaByobTest, LoadBinary) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.enable_seccomp_filter = param.enable_seccomp_filter}, param.mode);

  absl::StatusOr<std::string> code_id =
      roma_service.Register(kUdfPath / kCPlusPlusBinaryFilename,
                            /*num_workers=*/1);

  EXPECT_TRUE(code_id.status().ok()) << code_id.status();
}

TEST_P(RomaByobTest, ProcessRequestMultipleCppBinaries) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.enable_seccomp_filter = param.enable_seccomp_filter}, param.mode);

  std::string first_code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusBinaryFilename);
  std::string second_code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusNewBinaryFilename);

  EXPECT_THAT(
      SendRequestAndGetResponse(roma_service, first_code_token).greeting(),
      ::testing::StrEq(kFirstUdfOutput));
  EXPECT_THAT(
      SendRequestAndGetResponse(roma_service, second_code_token).greeting(),
      ::testing::StrEq(kNewUdfOutput));
}

TEST_P(RomaByobTest, LoadBinaryUsingUdfBlob) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.enable_seccomp_filter = param.enable_seccomp_filter}, param.mode);

  auto content = GetContentsOfFile(kUdfPath / kCPlusPlusBinaryFilename);
  CHECK_OK(content);
  auto udf_blob = UdfBlob::Create(*std::move(content));
  CHECK_OK(udf_blob);

  std::string first_code_token = LoadCode(roma_service, (*udf_blob)());

  EXPECT_THAT(
      SendRequestAndGetResponse(roma_service, first_code_token).greeting(),
      ::testing::StrEq(kFirstUdfOutput));
}

TEST_P(RomaByobTest, AsyncCallbackProcessRequestCppBinary) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.enable_seccomp_filter = param.enable_seccomp_filter}, param.mode);

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusBinaryFilename);

  // Data we are sending to the server.
  SampleRequest bin_request;
  bin_request.set_function(FUNCTION_HELLO_WORLD);
  absl::Notification notif;
  absl::StatusOr<SampleResponse> bin_response;
  auto callback = [&notif, &bin_response](absl::StatusOr<SampleResponse> resp) {
    bin_response = std::move(resp);
    notif.Notify();
  };

  CHECK_OK(roma_service.Sample(callback, std::move(bin_request),
                               /*metadata=*/{}, code_token));
  ASSERT_TRUE(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  CHECK_OK(bin_response);
  EXPECT_THAT(bin_response->greeting(), kFirstUdfOutput);
}

TEST_P(RomaByobTest, ProcessRequestGoLangBinary) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.lib_mounts = "", .enable_seccomp_filter = param.enable_seccomp_filter},
      param.mode);

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kGoLangBinaryFilename);

  EXPECT_THAT(SendRequestAndGetResponse(roma_service, code_token).greeting(),
              ::testing::StrEq(kGoBinaryOutput));
}

TEST_P(RomaByobTest, ProcessRequestJavaBinary) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
#if defined(__aarch64__)
  // TODO: b/377349908 - Enable Java benchmarks post-ARM64 fix
  GTEST_SKIP() << "Java tests disabled for ARM64";
#endif
  ByobSampleService<> roma_service =
      GetRomaService({.lib_mounts = "/proc",
                      .enable_seccomp_filter = param.enable_seccomp_filter},
                     param.mode);

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kJavaBinaryFilename);

  EXPECT_THAT(SendRequestAndGetResponse(roma_service, code_token).greeting(),
              ::testing::StrEq(kJavaBinaryOutput));
}

TEST_P(RomaByobTest, VerifyNoStdOutStdErrEgressionByDefault) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.enable_seccomp_filter = param.enable_seccomp_filter}, param.mode);

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusLogBinaryFilename);

  auto response_and_log_status =
      GetResponseAndLogStatus(roma_service, code_token);
  EXPECT_THAT(response_and_log_status.first.greeting(),
              ::testing::StrEq(kLogUdfOutput));
  EXPECT_EQ(response_and_log_status.second.code(), absl::StatusCode::kNotFound);
}

TEST_P(RomaByobTest, AsyncCallbackExecuteThenDeleteCppBinary) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.enable_seccomp_filter = param.enable_seccomp_filter}, param.mode);

  const std::string code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusPauseBinaryFilename);
  absl::Notification notif;

  CHECK_OK(roma_service.Sample(
      [&notif](absl::StatusOr<SampleResponse> /*resp*/) { notif.Notify(); },
      SampleRequest{},
      /*metadata=*/{}, code_token));
  EXPECT_FALSE(notif.WaitForNotificationWithTimeout(absl::Seconds(1)));

  roma_service.Delete(code_token);
  notif.WaitForNotification();
  const std::string second_code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusNewBinaryFilename);

  EXPECT_THAT(
      SendRequestAndGetResponse(roma_service, second_code_token).greeting(),
      ::testing::StrEq(kNewUdfOutput));
}

TEST_P(RomaByobTest, AsyncCallbackExecuteThenCancelCppBinary) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.enable_seccomp_filter = param.enable_seccomp_filter}, param.mode);

  const std::string code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusPauseBinaryFilename);
  absl::Notification notif;
  const auto execution_token = roma_service.Sample(
      [&notif](absl::StatusOr<SampleResponse> /*resp*/) { notif.Notify(); },
      SampleRequest{},
      /*metadata=*/{}, code_token);
  CHECK_OK(execution_token);

  EXPECT_FALSE(notif.WaitForNotificationWithTimeout(absl::Seconds(1)));
  roma_service.Cancel(*execution_token);
  CHECK(notif.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

TEST_P(RomaByobTest, VerifyStdOutStdErrEgressionByChoice) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.enable_seccomp_filter = param.enable_seccomp_filter}, param.mode);

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusLogBinaryFilename,
               /*enable_log_egress=*/true);

  auto response_and_logs = GetResponseAndLogs(roma_service, code_token);
  EXPECT_THAT(response_and_logs.first.greeting(),
              ::testing::StrEq(kLogUdfOutput));
  EXPECT_THAT(response_and_logs.second,
              ::testing::StrEq("I am a stdout log.\nI am a stderr log.\n"));
}

TEST_P(RomaByobTest, VerifyCodeTokenBasedLoadWorks) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.enable_seccomp_filter = param.enable_seccomp_filter}, param.mode);
  std::string no_log_code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusLogBinaryFilename);

  std::string log_code_token =
      LoadCodeFromCodeToken(roma_service, no_log_code_token);

  auto response_and_logs = GetResponseAndLogs(roma_service, log_code_token);
  EXPECT_THAT(response_and_logs.first.greeting(),
              ::testing::StrEq(kLogUdfOutput));
  EXPECT_THAT(response_and_logs.second,
              ::testing::StrEq("I am a stdout log.\nI am a stderr log.\n"));
}

TEST_P(RomaByobTest, VerifyRegisterWithAndWithoutLogs) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.enable_seccomp_filter = param.enable_seccomp_filter}, param.mode);
  std::string no_log_code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusLogBinaryFilename);

  std::string log_code_token =
      LoadCodeFromCodeToken(roma_service, no_log_code_token);

  auto response_and_logs = GetResponseAndLogs(roma_service, log_code_token);
  EXPECT_THAT(response_and_logs.first.greeting(),
              ::testing::StrEq(kLogUdfOutput));
  EXPECT_THAT(response_and_logs.second,
              ::testing::StrEq("I am a stdout log.\nI am a stderr log.\n"));

  absl::Notification exec_notif;
  absl::StatusOr<SampleResponse> bin_response;
  absl::Status log_status;
  auto callback = [&exec_notif, &bin_response, &log_status](
                      absl::StatusOr<SampleResponse> resp,
                      absl::StatusOr<std::string_view> logs) {
    bin_response = std::move(resp);
    CHECK(!logs.ok());
    // Making a copy -- try not to IRL.
    log_status = logs.status();
    exec_notif.Notify();
  };

  SampleRequest bin_request;
  CHECK_OK(roma_service.Sample(callback, std::move(bin_request),
                               /*metadata=*/{}, no_log_code_token));
  CHECK(exec_notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  CHECK_OK(bin_response);

  EXPECT_THAT(bin_response->greeting(), ::testing::StrEq(kLogUdfOutput));
  EXPECT_EQ(log_status.code(), absl::StatusCode::kNotFound);
}

TEST_P(RomaByobTest, VerifyHardLinkExecuteWorksAfterDeleteOriginal) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.enable_seccomp_filter = param.enable_seccomp_filter}, param.mode);
  std::string no_log_code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusLogBinaryFilename);

  absl::Notification exec_notif;
  absl::StatusOr<SampleResponse> bin_response;
  absl::Status log_status;
  auto callback = [&exec_notif, &bin_response, &log_status](
                      absl::StatusOr<SampleResponse> resp,
                      absl::StatusOr<std::string_view> logs) {
    bin_response = std::move(resp);
    CHECK(!logs.ok());
    // Making a copy -- try not to IRL.
    log_status = logs.status();
    exec_notif.Notify();
  };

  SampleRequest bin_request;
  CHECK_OK(roma_service.Sample(callback, std::move(bin_request),
                               /*metadata=*/{}, no_log_code_token));
  CHECK(exec_notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  CHECK_OK(bin_response);

  std::string log_code_token =
      LoadCodeFromCodeToken(roma_service, no_log_code_token);
  absl::SleepFor(absl::Milliseconds(25));

  roma_service.Delete(no_log_code_token);

  auto response_and_logs = GetResponseAndLogs(roma_service, log_code_token);
  EXPECT_THAT(response_and_logs.first.greeting(),
              ::testing::StrEq(kLogUdfOutput));
  EXPECT_THAT(response_and_logs.second,
              ::testing::StrEq("I am a stdout log.\nI am a stderr log.\n"));
  response_and_logs = GetResponseAndLogs(roma_service, log_code_token);
  EXPECT_THAT(response_and_logs.first.greeting(),
              ::testing::StrEq(kLogUdfOutput));
  EXPECT_THAT(response_and_logs.second,
              ::testing::StrEq("I am a stdout log.\nI am a stderr log.\n"));
}

TEST_P(RomaByobTest, VerifyNoCapabilities) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.enable_seccomp_filter = param.enable_seccomp_filter}, param.mode);

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusCapBinaryFilename);

  EXPECT_THAT(SendRequestAndGetResponse(roma_service, code_token).greeting(),
              ::testing::StrEq("Empty capabilities set as expected."));
}

TEST_P(RomaByobTest, VerifySyscallFilter) {
  RomaByobTestParam param = GetParam();
  if (!param.enable_seccomp_filter) {
    GTEST_SKIP() << "Seccomp filtering is disabled";
  }
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(
      {.enable_seccomp_filter = param.enable_seccomp_filter}, param.mode);

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusSyscallFilterBinaryFilename);

  EXPECT_THAT(SendRequestAndGetResponse(roma_service, code_token).greeting(),
              ::testing::StrEq("Blocked dup."));
}

std::string GetModeStr(Mode mode) {
  switch (mode) {
    case Mode::kModeGvisorSandbox:
      return "GvisorSandbox";
    case Mode::kModeMinimalSandbox:
      return "MinimalSandbox";
    case Mode::kModeGvisorSandboxDebug:
      return "GvisorSandboxDebug";
    case Mode::kModeNsJailSandbox:
      return "NsJailSandbox";
    default:
      return "UnknownMode";
  }
}

std::string GetFilterStr(bool enable_seccomp_filter) {
  if (enable_seccomp_filter) {
    return "SeccompFilterEnabled";
  } else {
    return "SeccompFilterDisabled";
  }
}

INSTANTIATE_TEST_SUITE_P(
    RomaByobTestSuiteInstantiation, RomaByobTest,
    // Since we don't want to run tests for gVisor debug mode,
    // Mode::kModeGvisorSandboxDebug has not included in the list.
    testing::ValuesIn<RomaByobTestParam>(
        {{.mode = Mode::kModeGvisorSandbox, .enable_seccomp_filter = false},
         {.mode = Mode::kModeMinimalSandbox, .enable_seccomp_filter = false},
         {.mode = Mode::kModeNsJailSandbox, .enable_seccomp_filter = false},
         {.mode = Mode::kModeGvisorSandbox, .enable_seccomp_filter = true},
         {.mode = Mode::kModeMinimalSandbox, .enable_seccomp_filter = true},
         {.mode = Mode::kModeNsJailSandbox, .enable_seccomp_filter = true}}),
    [](const testing::TestParamInfo<RomaByobTest::ParamType>& info) {
      return absl::StrCat(GetModeStr(info.param.mode),
                          GetFilterStr(info.param.enable_seccomp_filter));
    });
}  // namespace
}  // namespace privacy_sandbox::server_common::byob::test
