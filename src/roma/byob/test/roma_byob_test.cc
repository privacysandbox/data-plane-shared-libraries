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

#include "absl/log/initialize.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "src/roma/byob/sample_udf/sample_callback.pb.h"
#include "src/roma/byob/sample_udf/sample_roma_byob_app_service.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"
#include "src/roma/byob/utility/udf_blob.h"
#include "src/roma/byob/utility/utils.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/util/status_macro/status_macros.h"

namespace privacy_sandbox::server_common::byob::test {

namespace {
using ::privacy_sandbox::roma_byob::example::ByobSampleService;
using ::privacy_sandbox::roma_byob::example::FUNCTION_HELLO_WORLD;
using ::privacy_sandbox::roma_byob::example::FUNCTION_PRIME_SIEVE;
using ::privacy_sandbox::roma_byob::example::FUNCTION_READ_SYS_V_MESSAGE_QUEUE;
using ::privacy_sandbox::roma_byob::example::
    FUNCTION_RELOADER_LEVEL_SYSCALL_FILTERING;
using ::privacy_sandbox::roma_byob::example::
    FUNCTION_WORKER_AND_RELOADER_LEVEL_SYSCALL_FILTERING;
using ::privacy_sandbox::roma_byob::example::FUNCTION_WRITE_SYS_V_MESSAGE_QUEUE;
using ::privacy_sandbox::roma_byob::example::FunctionType;
using ::privacy_sandbox::roma_byob::example::SampleRequest;
using ::privacy_sandbox::roma_byob::example::SampleResponse;
using ::privacy_sandbox::server_common::byob::HasClonePermissionsByobWorker;
using ::privacy_sandbox::server_common::byob::Mode;
using ::privacy_sandbox::server_common::byob::SyscallFiltering;
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
const std::filesystem::path kCPlusPlusSyscallFilteringBinaryFilename =
    "syscall_filter_udf";
const std::filesystem::path kCPlusPlusNewBinaryFilename = "new_udf";
const std::filesystem::path kCPlusPlusLogBinaryFilename = "log_udf";
const std::filesystem::path kCPlusPlusPauseBinaryFilename = "pause_udf";
constexpr std::string_view kFirstUdfOutput = "Hello, world!";
constexpr std::string_view kNewUdfOutput = "I am a new UDF!";
constexpr std::string_view kGoBinaryOutput = "Hello, world from Go!";
constexpr std::string_view kJavaBinaryOutput = "Hello, world from Java!";
constexpr std::string_view kLogUdfOutput = "I am a UDF that logs.";
constexpr absl::Duration kTimeout = absl::Seconds(1);

absl::StatusOr<SampleResponse> SendRequestAndGetResponse(
    ByobSampleService<>& roma_service, std::string_view code_token,
    FunctionType func_type = FUNCTION_HELLO_WORLD) {
  // Data we are sending to the server.
  SampleRequest bin_request;
  bin_request.set_function(func_type);
  absl::StatusOr<std::unique_ptr<SampleResponse>> response;

  absl::Notification notif;
  PS_ASSIGN_OR_RETURN(
      auto exec_token,
      roma_service.Sample(notif, std::move(bin_request), response,
                          /*metadata=*/{}, code_token, kTimeout));
  if (!notif.WaitForNotificationWithTimeout(kTimeout)) {
    return absl::InternalError("Failed to execute request within timeout");
  }
  if (response.ok()) {
    return std::move(**std::move(response));
  } else {
    return std::move(response).status();
  }
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

absl::StatusOr<std::string> LoadCode(ByobSampleService<>& roma_service,
                                     std::filesystem::path file_path,
                                     bool enable_log_egress = false,
                                     int num_workers = 20) {
  if (!enable_log_egress) {
    return roma_service.Register(file_path, num_workers);
  } else {
    return roma_service.RegisterForLogging(file_path, num_workers);
  }
}

absl::StatusOr<std::string> LoadCodeFromCodeToken(
    ByobSampleService<>& roma_service, std::string no_log_code_token,
    int num_workers = 20) {
  return roma_service.RegisterForLogging(no_log_code_token,
                                         /*num_workers=*/num_workers);
}

absl::StatusOr<ByobSampleService<>> GetRomaService(
    ::privacy_sandbox::server_common::byob::Config<> config, Mode mode) {
  absl::StatusOr<ByobSampleService<>> sample_interface =
      ByobSampleService<>::Create(std::move(config), mode);
  return sample_interface;
}

absl::StatusOr<std::pair<SampleResponse, std::string>> GetResponseAndLogs(
    ByobSampleService<>& roma_service, std::string_view code_token,
    FunctionType func_type = FUNCTION_HELLO_WORLD) {
  absl::Notification exec_notif;
  absl::StatusOr<SampleResponse> bin_response;
  absl::StatusOr<std::string> logs_acquired;
  auto callback = [&exec_notif, &bin_response, &logs_acquired](
                      absl::StatusOr<SampleResponse> resp,
                      absl::StatusOr<std::string_view> logs,
                      ProcessRequestMetrics /*metrics*/) {
    bin_response = std::move(resp);
    // Making a copy -- try not to IRL.
    logs_acquired = std::move(logs);
    exec_notif.Notify();
  };

  SampleRequest bin_request;
  bin_request.set_function(func_type);
  PS_ASSIGN_OR_RETURN(
      auto exec_token,
      roma_service.Sample(callback, std::move(bin_request),
                          /*metadata=*/{}, code_token, kTimeout));
  if (!exec_notif.WaitForNotificationWithTimeout(kTimeout)) {
    return absl::InternalError("Failed to execute request within timeout");
  }
  if (!bin_response.ok()) {
    return std::move(bin_response).status();
  }
  if (!logs_acquired.ok()) {
    return std::move(logs_acquired).status();
  }
  return std::pair<SampleResponse, std::string>(*std::move(bin_response),
                                                *std::move(logs_acquired));
}

absl::StatusOr<std::pair<SampleResponse, absl::Status>> GetResponseAndLogStatus(
    ByobSampleService<>& roma_service, std::string_view code_token) {
  absl::Notification exec_notif;
  absl::StatusOr<SampleResponse> bin_response;
  absl::Status log_status;
  auto callback = [&exec_notif, &bin_response, &log_status](
                      absl::StatusOr<SampleResponse> resp,
                      absl::StatusOr<std::string_view> logs,
                      ProcessRequestMetrics /*metrics*/) {
    bin_response = std::move(resp);
    log_status = std::move(logs.status());
    exec_notif.Notify();
  };

  SampleRequest bin_request;
  PS_ASSIGN_OR_RETURN(
      auto exec_token,
      roma_service.Sample(callback, std::move(bin_request),
                          /*metadata=*/{}, code_token, kTimeout));
  if (!exec_notif.WaitForNotificationWithTimeout(kTimeout)) {
    return absl::InternalError("Failed to execute request within timeout");
  }
  if (!bin_response.ok()) {
    return std::move(bin_response).status();
  }
  return std::pair<SampleResponse, absl::Status>(*std::move(bin_response),
                                                 log_status);
}

struct RomaByobTestParam {
  Mode mode;
  SyscallFiltering syscall_filtering;
};
using RomaByobTest = TestWithParam<RomaByobTestParam>;

TEST_P(RomaByobTest, NoSocketFile) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto code_token =
      LoadCode(*roma_service, kUdfPath / kCPlusPlusSocketFinderBinaryFilename,
               /*enable_log_egress=*/true, /*num_workers=*/1);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  auto response = SendRequestAndGetResponse(*roma_service, *code_token);
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_THAT(response->greeting(), ::testing::StrEq("Success."));
}

TEST_P(RomaByobTest, SystemVMessageQueueEgressDisabledIpcNamespaceEnabled) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  if (param.syscall_filtering != SyscallFiltering::kNoSyscallFiltering) {
    GTEST_SKIP() << "System V Message Queue system calls are disabled when "
                    "syscall filtering is enabled";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto code_token = LoadCode(*roma_service, kUdfPath / "message_queue_udf",
                             /*enable_log_egress=*/true, /*num_workers=*/4);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  auto response_and_logs = GetResponseAndLogs(
      *roma_service, *code_token, FUNCTION_WRITE_SYS_V_MESSAGE_QUEUE);
  ASSERT_TRUE(response_and_logs.ok()) << response_and_logs.status();

  EXPECT_THAT(response_and_logs->first.greeting(),
              ::testing::StrEq("Hello from System V message queue IPC."));
  response_and_logs = GetResponseAndLogs(*roma_service, *code_token,
                                         FUNCTION_READ_SYS_V_MESSAGE_QUEUE);
  ASSERT_TRUE(response_and_logs.ok()) << response_and_logs.status();
  EXPECT_THAT(response_and_logs->first.greeting(),
              ::testing::StrEq("Could not find a message in the queue"));
}

TEST_P(RomaByobTest, SystemVMessageQueueEgressDisabledIpcNamespaceDisabled) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  if (param.syscall_filtering == SyscallFiltering::kNoSyscallFiltering) {
    GTEST_SKIP() << "Other scenario covered in "
                    "SystemVMessageQueueEgressDisabledIpcNamespaceEnabled";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto code_token = LoadCode(*roma_service, kUdfPath / "message_queue_udf",
                             /*enable_log_egress=*/true, /*num_workers=*/4);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  auto response_and_logs = GetResponseAndLogs(
      *roma_service, *code_token, FUNCTION_WRITE_SYS_V_MESSAGE_QUEUE);
  ASSERT_TRUE(response_and_logs.ok()) << response_and_logs.status();
  EXPECT_THAT(
      response_and_logs->first.greeting(),
      ::testing::StrEq("Failed to send message: Operation not permitted"));
  response_and_logs = GetResponseAndLogs(*roma_service, *code_token,
                                         FUNCTION_READ_SYS_V_MESSAGE_QUEUE);
  EXPECT_THAT(
      response_and_logs->first.greeting(),
      ::testing::StrEq("Failed to receive message: Operation not permitted"));
}

TEST_P(RomaByobTest, NoFileSystemCreateEgression) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto code_token =
      LoadCode(*roma_service, kUdfPath / kCPlusPlusFileSystemAddFilename,
               /*enable_log_egress=*/true, /*num_workers=*/1);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  auto response = SendRequestAndGetResponse(*roma_service, *code_token);
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_THAT(response->greeting(), ::testing::StrEq("Success."));
}

TEST_P(RomaByobTest, NoFileSystemDeleteEgression) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto code_token =
      LoadCode(*roma_service, kUdfPath / kCPlusPlusFileSystemDeleteFilename,
               /*enable_log_egress=*/true, /*num_workers=*/1);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  auto response = SendRequestAndGetResponse(*roma_service, *code_token);
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_THAT(response->greeting(), ::testing::StrEq("Success."));
}

TEST_P(RomaByobTest, NoFileSystemEditEgression) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  if (param.mode == Mode::kModeGvisorSandbox) {
    GTEST_SKIP() << "Executing the binary is failing: Exec format error [8]";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto code_token =
      LoadCode(*roma_service, kUdfPath / kCPlusPlusFileSystemEditFilename,
               /*enable_log_egress=*/true, /*num_workers=*/1);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  auto response = SendRequestAndGetResponse(*roma_service, *code_token);
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_THAT(response->greeting(), ::testing::StrEq("Success."));
}

TEST_P(RomaByobTest, LoadBinary) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  absl::StatusOr<std::string> code_id =
      roma_service->Register(kUdfPath / kCPlusPlusBinaryFilename,
                             /*num_workers=*/1);

  EXPECT_TRUE(code_id.status().ok()) << code_id.status();
}

TEST_P(RomaByobTest, ProcessRequestMultipleCppBinaries) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto first_code_token =
      LoadCode(*roma_service, kUdfPath / kCPlusPlusBinaryFilename);
  ASSERT_TRUE(first_code_token.ok()) << first_code_token.status();
  auto second_code_token =
      LoadCode(*roma_service, kUdfPath / kCPlusPlusNewBinaryFilename);
  ASSERT_TRUE(second_code_token.ok()) << second_code_token.status();

  auto response = SendRequestAndGetResponse(*roma_service, *first_code_token);
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_THAT(response->greeting(), ::testing::StrEq(kFirstUdfOutput));
  response = SendRequestAndGetResponse(*roma_service, *second_code_token);
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_THAT(response->greeting(), ::testing::StrEq(kNewUdfOutput));
}

TEST_P(RomaByobTest, LoadBinaryUsingUdfBlob) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto content = GetContentsOfFile(kUdfPath / kCPlusPlusBinaryFilename);
  ASSERT_TRUE(content.ok()) << content.status();
  auto udf_blob = UdfBlob::Create(*std::move(content));
  ASSERT_TRUE(udf_blob.ok()) << udf_blob.status();

  auto first_code_token = LoadCode(*roma_service, (*udf_blob)());
  ASSERT_TRUE(first_code_token.ok()) << first_code_token.status();

  auto response = SendRequestAndGetResponse(*roma_service, *first_code_token);
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_THAT(response->greeting(), ::testing::StrEq(kFirstUdfOutput));
}

TEST_P(RomaByobTest, AsyncCallbackProcessRequestCppBinary) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto code_token =
      LoadCode(*roma_service, kUdfPath / kCPlusPlusBinaryFilename);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  // Data we are sending to the server.
  SampleRequest bin_request;
  bin_request.set_function(FUNCTION_HELLO_WORLD);
  absl::Notification notif;
  absl::StatusOr<SampleResponse> bin_response;
  auto callback = [&notif, &bin_response](
                      absl::StatusOr<SampleResponse> resp,
                      absl::StatusOr<std::string_view> /*logs*/,
                      ProcessRequestMetrics /*metrics*/) {
    bin_response = std::move(resp);
    notif.Notify();
  };

  ASSERT_TRUE(roma_service
                  ->Sample(callback, std::move(bin_request),
                           /*metadata=*/{}, *code_token)
                  .ok());
  ASSERT_TRUE(notif.WaitForNotificationWithTimeout(absl::Seconds(1)));
  ASSERT_TRUE(bin_response.ok());
  EXPECT_THAT(bin_response->greeting(), kFirstUdfOutput);
}

TEST_P(RomaByobTest, ProcessRequestGoLangBinary) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto roma_service = GetRomaService(
      {.lib_mounts = "", .syscall_filtering = param.syscall_filtering},
      param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto code_token = LoadCode(*roma_service, kUdfPath / kGoLangBinaryFilename);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  auto response = SendRequestAndGetResponse(*roma_service, *code_token);
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_THAT(response->greeting(), ::testing::StrEq(kGoBinaryOutput));
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
  auto roma_service = GetRomaService(
      {.lib_mounts = "/proc", .syscall_filtering = param.syscall_filtering},
      param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto code_token = LoadCode(*roma_service, kUdfPath / kJavaBinaryFilename);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  auto response = SendRequestAndGetResponse(*roma_service, *code_token);
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_THAT(response->greeting(), ::testing::StrEq(kJavaBinaryOutput));
}

TEST_P(RomaByobTest, VerifyNoStdOutStdErrEgressionByDefault) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto code_token =
      LoadCode(*roma_service, kUdfPath / kCPlusPlusLogBinaryFilename);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  auto response_and_log_status =
      GetResponseAndLogStatus(*roma_service, *code_token);
  ASSERT_TRUE(response_and_log_status.ok());
  EXPECT_THAT((response_and_log_status->first).greeting(),
              ::testing::StrEq(kLogUdfOutput));
  EXPECT_FALSE((response_and_log_status->second).ok());
}

TEST_P(RomaByobTest, AsyncCallbackExecuteThenDeleteCppBinary) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto code_token =
      LoadCode(*roma_service, kUdfPath / kCPlusPlusPauseBinaryFilename);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  absl::Notification notif;
  ASSERT_TRUE(
      roma_service
          ->Sample(
              [&notif](absl::StatusOr<SampleResponse> /*resp*/,
                       absl::StatusOr<std::string_view> /*logs*/,
                       ProcessRequestMetrics /*metrics*/) { notif.Notify(); },
              SampleRequest{},
              /*metadata=*/{}, *code_token)
          .ok());
  ASSERT_FALSE(notif.WaitForNotificationWithTimeout(absl::Seconds(1)));

  roma_service->Delete(*code_token);
  ASSERT_TRUE(notif.WaitForNotificationWithTimeout(absl::Seconds(2)));
  auto second_code_token =
      LoadCode(*roma_service, kUdfPath / kCPlusPlusNewBinaryFilename);
  ASSERT_TRUE(second_code_token.ok()) << second_code_token.status();

  auto response = SendRequestAndGetResponse(*roma_service, *second_code_token);
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_THAT(response->greeting(), ::testing::StrEq(kNewUdfOutput));
}

TEST_P(RomaByobTest, AsyncCallbackExecuteThenCancelCppBinary) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto code_token =
      LoadCode(*roma_service, kUdfPath / kCPlusPlusPauseBinaryFilename);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  absl::Notification notif;
  const auto exec_token = roma_service->Sample(
      [&notif](absl::StatusOr<SampleResponse> /*resp*/,
               absl::StatusOr<std::string_view> /*logs*/,
               ProcessRequestMetrics /*metrics*/) { notif.Notify(); },
      SampleRequest{},
      /*metadata=*/{}, *code_token);
  ASSERT_TRUE(exec_token.ok());

  ASSERT_FALSE(notif.WaitForNotificationWithTimeout(absl::Seconds(1)));
  roma_service->Cancel(*exec_token);
  ASSERT_TRUE(notif.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

TEST_P(RomaByobTest, VerifyStdOutStdErrEgressionByChoice) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto code_token =
      LoadCode(*roma_service, kUdfPath / kCPlusPlusLogBinaryFilename,
               /*enable_log_egress=*/true);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  auto response_and_logs = GetResponseAndLogs(*roma_service, *code_token);
  ASSERT_TRUE(response_and_logs.ok()) << response_and_logs.status();
  EXPECT_THAT(response_and_logs->first.greeting(),
              ::testing::StrEq(kLogUdfOutput));
  EXPECT_THAT(response_and_logs->second,
              ::testing::StrEq("I am a stdout log.\nI am a stderr log.\n"));
}

TEST_P(RomaByobTest, VerifyCodeTokenBasedLoadWorks) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto no_log_code_token =
      LoadCode(*roma_service, kUdfPath / kCPlusPlusLogBinaryFilename);
  ASSERT_TRUE(no_log_code_token.ok()) << no_log_code_token.status();

  auto log_code_token =
      LoadCodeFromCodeToken(*roma_service, *no_log_code_token);
  ASSERT_TRUE(log_code_token.ok()) << log_code_token.status();

  auto response_and_logs = GetResponseAndLogs(*roma_service, *log_code_token);
  ASSERT_TRUE(response_and_logs.ok()) << response_and_logs.status();
  EXPECT_THAT(response_and_logs->first.greeting(),
              ::testing::StrEq(kLogUdfOutput));
  EXPECT_THAT(response_and_logs->second,
              ::testing::StrEq("I am a stdout log.\nI am a stderr log.\n"));
}

TEST_P(RomaByobTest, VerifyRegisterWithAndWithoutLogs) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto no_log_code_token =
      LoadCode(*roma_service, kUdfPath / kCPlusPlusLogBinaryFilename);
  ASSERT_TRUE(no_log_code_token.ok()) << no_log_code_token.status();

  auto log_code_token =
      LoadCodeFromCodeToken(*roma_service, *no_log_code_token);
  ASSERT_TRUE(log_code_token.ok()) << log_code_token.status();

  auto response_and_logs = GetResponseAndLogs(*roma_service, *log_code_token);
  EXPECT_THAT(response_and_logs->first.greeting(),
              ::testing::StrEq(kLogUdfOutput));
  EXPECT_THAT(response_and_logs->second,
              ::testing::StrEq("I am a stdout log.\nI am a stderr log.\n"));

  absl::Notification exec_notif;
  absl::StatusOr<SampleResponse> bin_response;
  absl::Status log_status;
  auto callback = [&exec_notif, &bin_response, &log_status](
                      absl::StatusOr<SampleResponse> resp,
                      absl::StatusOr<std::string_view> logs,
                      ProcessRequestMetrics /*metrics*/) {
    bin_response = std::move(resp);
    log_status = logs.status();
    exec_notif.Notify();
  };

  SampleRequest bin_request;
  ASSERT_TRUE(roma_service
                  ->Sample(callback, std::move(bin_request),
                           /*metadata=*/{}, *no_log_code_token)
                  .ok());
  ASSERT_TRUE(exec_notif.WaitForNotificationWithTimeout(absl::Seconds(1)));
  ASSERT_TRUE(bin_response.ok());

  EXPECT_THAT(bin_response->greeting(), ::testing::StrEq(kLogUdfOutput));
  EXPECT_FALSE(log_status.ok());
}

TEST_P(RomaByobTest, VerifyHardLinkExecuteWorksAfterDeleteOriginal) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto no_log_code_token =
      LoadCode(*roma_service, kUdfPath / kCPlusPlusLogBinaryFilename);
  ASSERT_TRUE(no_log_code_token.ok()) << no_log_code_token.status();

  absl::Notification exec_notif;
  absl::StatusOr<SampleResponse> bin_response;
  absl::Status log_status;
  auto callback = [&exec_notif, &bin_response, &log_status](
                      absl::StatusOr<SampleResponse> resp,
                      absl::StatusOr<std::string_view> logs,
                      ProcessRequestMetrics /*metrics*/) {
    bin_response = std::move(resp);
    log_status = logs.status();
    exec_notif.Notify();
  };

  SampleRequest bin_request;
  ASSERT_TRUE(roma_service
                  ->Sample(callback, std::move(bin_request),
                           /*metadata=*/{}, *no_log_code_token)
                  .ok());
  ASSERT_TRUE(exec_notif.WaitForNotificationWithTimeout(absl::Seconds(1)));
  ASSERT_TRUE(bin_response.ok());

  auto log_code_token =
      LoadCodeFromCodeToken(*roma_service, *no_log_code_token);
  ASSERT_TRUE(log_code_token.ok()) << log_code_token.status();

  roma_service->Delete(*no_log_code_token);

  auto response_and_logs = GetResponseAndLogs(*roma_service, *log_code_token);
  ASSERT_TRUE(response_and_logs.ok()) << response_and_logs.status();
  EXPECT_THAT(response_and_logs->first.greeting(),
              ::testing::StrEq(kLogUdfOutput));
  EXPECT_THAT(response_and_logs->second,
              ::testing::StrEq("I am a stdout log.\nI am a stderr log.\n"));
  response_and_logs = GetResponseAndLogs(*roma_service, *log_code_token);
  ASSERT_TRUE(response_and_logs.ok()) << response_and_logs.status();
  EXPECT_THAT(response_and_logs->first.greeting(),
              ::testing::StrEq(kLogUdfOutput));
  EXPECT_THAT(response_and_logs->second,
              ::testing::StrEq("I am a stdout log.\nI am a stderr log.\n"));
}

TEST_P(RomaByobTest, VerifyNoCapabilities) {
  RomaByobTestParam param = GetParam();
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto code_token =
      LoadCode(*roma_service, kUdfPath / kCPlusPlusCapBinaryFilename);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  auto response = SendRequestAndGetResponse(*roma_service, *code_token);
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_THAT(response->greeting(),
              ::testing::StrEq("Empty capabilities set as expected."));
}

TEST_P(RomaByobTest, VerifySyscallFiltering) {
  RomaByobTestParam param = GetParam();
  if (param.syscall_filtering == SyscallFiltering::kNoSyscallFiltering) {
    GTEST_SKIP() << "Seccomp filtering is disabled";
  }
  if (!HasClonePermissionsByobWorker(param.mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto roma_service = GetRomaService(
      {.syscall_filtering = param.syscall_filtering}, param.mode);
  ASSERT_TRUE(roma_service.ok()) << roma_service.status();

  auto code_token = LoadCode(
      *roma_service, kUdfPath / kCPlusPlusSyscallFilteringBinaryFilename,
      /*enable_log_egress=*/true);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  auto response = SendRequestAndGetResponse(
      *roma_service, *code_token,
      param.syscall_filtering ==
              SyscallFiltering::kUntrustedCodeSyscallFiltering
          ? FUNCTION_WORKER_AND_RELOADER_LEVEL_SYSCALL_FILTERING
          : FUNCTION_RELOADER_LEVEL_SYSCALL_FILTERING);
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_THAT(response->greeting(),
              ::testing::StrEq("Blocked all System V IPC syscalls and handled "
                               "syscall filters correctly."));
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

std::string GetFilterStr(SyscallFiltering syscall_filtering) {
  switch (syscall_filtering) {
    case SyscallFiltering::kNoSyscallFiltering:
      return "NoSyscallFiltering";
    case SyscallFiltering::kWorkerEngineSyscallFiltering:
      return "WorkerEngineSyscallFiltering";
    case SyscallFiltering::kUntrustedCodeSyscallFiltering:
      return "UntrustedCodeSyscallFiltering";
    default:
      return "UnknownSyscallFiltering";
  }
}

INSTANTIATE_TEST_SUITE_P(
    RomaByobTestSuiteInstantiation, RomaByobTest,
    // Since we don't want to run tests for gVisor debug mode,
    // Mode::kModeGvisorSandboxDebug has not included in the list.
    testing::ValuesIn<RomaByobTestParam>(
        {{.mode = Mode::kModeGvisorSandbox,
          .syscall_filtering = SyscallFiltering::kNoSyscallFiltering},
         {.mode = Mode::kModeMinimalSandbox,
          .syscall_filtering = SyscallFiltering::kNoSyscallFiltering},
         {.mode = Mode::kModeNsJailSandbox,
          .syscall_filtering = SyscallFiltering::kNoSyscallFiltering},
         {.mode = Mode::kModeGvisorSandbox,
          .syscall_filtering =
              SyscallFiltering::kUntrustedCodeSyscallFiltering},
         {.mode = Mode::kModeMinimalSandbox,
          .syscall_filtering =
              SyscallFiltering::kUntrustedCodeSyscallFiltering},
         {.mode = Mode::kModeNsJailSandbox,
          .syscall_filtering =
              SyscallFiltering::kUntrustedCodeSyscallFiltering},
         {.mode = Mode::kModeNsJailSandbox,
          .syscall_filtering =
              SyscallFiltering::kWorkerEngineSyscallFiltering}}),
    [](const testing::TestParamInfo<RomaByobTest::ParamType>& info) {
      return absl::StrCat(GetModeStr(info.param.mode),
                          GetFilterStr(info.param.syscall_filtering));
    });
}  // namespace
}  // namespace privacy_sandbox::server_common::byob::test

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
