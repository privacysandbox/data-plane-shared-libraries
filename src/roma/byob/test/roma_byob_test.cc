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
using ::google::scp::roma::FunctionBindingObjectV2;
using ::privacy_sandbox::roma_byob::example::ByobSampleService;
using ::privacy_sandbox::roma_byob::example::FUNCTION_CALLBACK;
using ::privacy_sandbox::roma_byob::example::FUNCTION_HELLO_WORLD;
using ::privacy_sandbox::roma_byob::example::FUNCTION_PRIME_SIEVE;
using ::privacy_sandbox::roma_byob::example::FUNCTION_TEN_CALLBACK_INVOCATIONS;
using ::privacy_sandbox::roma_byob::example::FunctionType;
using ::privacy_sandbox::roma_byob::example::ReadCallbackPayloadRequest;
using ::privacy_sandbox::roma_byob::example::ReadCallbackPayloadResponse;
using ::privacy_sandbox::roma_byob::example::SampleRequest;
using ::privacy_sandbox::roma_byob::example::SampleResponse;
using ::privacy_sandbox::server_common::byob::CallbackReadRequest;
using ::privacy_sandbox::server_common::byob::CallbackReadResponse;
using ::privacy_sandbox::server_common::byob::HasClonePermissionsByobWorker;
using ::privacy_sandbox::server_common::byob::Mode;

const std::filesystem::path kUdfPath = "/udf";
const std::filesystem::path kGoLangBinaryFilename = "sample_go_udf";
const std::filesystem::path kCPlusPlusBinaryFilename = "sample_udf";
const std::filesystem::path kCPlusPlusNewBinaryFilename = "new_udf";
const std::filesystem::path kCPlusPlusLogBinaryFilename = "log_udf";
const std::filesystem::path kCPlusPlusPauseBinaryFilename = "pause_udf";
const std::filesystem::path kCallbackPayloadReadUdfFilename =
    "callback_payload_read_udf";
constexpr std::string_view kFirstUdfOutput = "Hello, world!";
constexpr std::string_view kNewUdfOutput = "I am a new UDF!";
constexpr std::string_view kGoBinaryOutput = "Hello, world from Go!";
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
                     bool enable_log_egress = false, int num_workers = 1) {
  absl::Notification notif;
  absl::Status notif_status;
  absl::StatusOr<std::string> code_id;
  if (!enable_log_egress) {
    code_id =
        roma_service.Register(file_path, notif, notif_status, num_workers);
  } else {
    code_id = roma_service.RegisterForLogging(file_path, notif, notif_status,
                                              num_workers);
  }
  CHECK_OK(code_id);
  CHECK(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  CHECK_OK(notif_status);
  return *std::move(code_id);
}

std::string LoadCodeFromCodeToken(ByobSampleService<>& roma_service,
                                  std::string no_log_code_token,
                                  int num_workers = 1) {
  absl::Notification notif;
  absl::Status notif_status;
  absl::StatusOr<std::string> code_id =
      roma_service.RegisterForLogging(no_log_code_token, notif, notif_status,
                                      /*num_workers=*/num_workers);
  CHECK_OK(code_id);
  CHECK(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  CHECK_OK(notif_status);
  return *std::move(code_id);
}

ByobSampleService<> GetRomaService(Mode mode) {
  privacy_sandbox::server_common::byob::Config<> config = {
      .roma_container_name = "roma_server",
      .function_bindings = {FunctionBindingObjectV2<>{"example", [](auto&) {}}},
  };
  absl::StatusOr<ByobSampleService<>> sample_interface =
      ByobSampleService<>::Create(config, mode);
  CHECK_OK(sample_interface);
  return std::move(*sample_interface);
}

void ReadCallbackPayload(google::scp::roma::FunctionBindingPayload<>& wrapper) {
  CallbackReadRequest req;
  CHECK(req.ParseFromString(wrapper.io_proto.input_bytes()));
  int64_t payload_size = 0;
  for (const auto& p : req.payloads()) {
    payload_size += p.size();
  }
  CallbackReadResponse resp;
  resp.set_payload_size(payload_size);
  wrapper.io_proto.clear_input_bytes();
  resp.SerializeToString(wrapper.io_proto.mutable_output_bytes());
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

TEST(RomaByobTest, LoadBinaryInSandboxMode) {
  ByobSampleService<> roma_service = GetRomaService(Mode::kModeSandbox);

  absl::Notification notif;
  absl::Status notif_status;
  absl::StatusOr<std::string> code_id =
      roma_service.Register(kUdfPath / kCPlusPlusBinaryFilename, notif,
                            notif_status, /*num_workers=*/1);

  EXPECT_TRUE(code_id.status().ok());
  EXPECT_TRUE(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  EXPECT_TRUE(notif_status.ok());
}

TEST(RomaByobTest, LoadBinaryInNonSandboxMode) {
  Mode mode = Mode::kModeNoSandbox;
  if (!HasClonePermissionsByobWorker(mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(mode);

  absl::Notification notif;
  absl::Status notif_status;
  absl::StatusOr<std::string> code_id =
      roma_service.Register(kUdfPath / kCPlusPlusBinaryFilename, notif,
                            notif_status, /*num_workers=*/1);

  EXPECT_TRUE(code_id.status().ok());
  EXPECT_TRUE(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  EXPECT_TRUE(notif_status.ok());
}

TEST(RomaByobTest, ProcessRequestMultipleCppBinariesInSandboxMode) {
  ByobSampleService<> roma_service = GetRomaService(Mode::kModeSandbox);

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

TEST(RomaByobTest, ProcessRequestMultipleCppBinariesInNonSandboxMode) {
  Mode mode = Mode::kModeNoSandbox;
  if (!HasClonePermissionsByobWorker(mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(mode);

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

TEST(RomaByobTest, LoadBinaryUsingUdfBlob) {
  Mode mode = Mode::kModeNoSandbox;
  if (!HasClonePermissionsByobWorker(mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  ByobSampleService<> roma_service = GetRomaService(mode);

  auto content = GetContentsOfFile(kUdfPath / kCPlusPlusBinaryFilename);
  CHECK_OK(content);
  auto udf_blob = UdfBlob::Create(*std::move(content));
  CHECK_OK(udf_blob);

  std::string first_code_token = LoadCode(roma_service, (*udf_blob)());

  EXPECT_THAT(
      SendRequestAndGetResponse(roma_service, first_code_token).greeting(),
      ::testing::StrEq(kFirstUdfOutput));
}

TEST(RomaByobTest, AsyncCallbackProcessRequestCppBinary) {
  ByobSampleService<> roma_service = GetRomaService(Mode::kModeSandbox);

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

TEST(RomaByobTest, ProcessRequestGoLangBinaryInSandboxMode) {
  ByobSampleService<> roma_service = GetRomaService(Mode::kModeSandbox);

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kGoLangBinaryFilename);

  EXPECT_THAT(SendRequestAndGetResponse(roma_service, code_token).greeting(),
              ::testing::StrEq(kGoBinaryOutput));
}

TEST(RomaByobTest, ProcessRequestCppBinaryWithHostCallbackInSandboxMode) {
  int64_t elem_size = 5;
  int64_t elem_count = 10;
  ::privacy_sandbox::server_common::byob::Config<> config = {
      .roma_container_name = "roma_server",
      .function_bindings = {FunctionBindingObjectV2<>{"example",
                                                      ReadCallbackPayload}},
  };
  absl::StatusOr<ByobSampleService<>> sample_interface =
      ByobSampleService<>::Create(config);
  CHECK_OK(sample_interface);
  ByobSampleService<> roma_service = std::move(*sample_interface);
  ReadCallbackPayloadRequest request;
  request.set_element_size(elem_size);
  request.set_element_count(elem_count);
  const int64_t payload_size = elem_size * elem_count;

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kCallbackPayloadReadUdfFilename);
  absl::StatusOr<std::unique_ptr<ReadCallbackPayloadResponse>> response;
  absl::Notification notif;
  CHECK_OK(roma_service.ReadCallbackPayload(notif, std::move(request), response,
                                            /*metadata=*/{}, code_token));

  ASSERT_TRUE(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  ASSERT_TRUE(response.ok());
  EXPECT_EQ((*response)->payload_size(), payload_size);
}

TEST(RomaByobTest, VerifyNoStdOutStdErrEgressionByDefault) {
  ByobSampleService<> roma_service = GetRomaService(Mode::kModeSandbox);

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusLogBinaryFilename);

  EXPECT_THAT(SendRequestAndGetResponse(roma_service, code_token).greeting(),
              ::testing::StrEq(kLogUdfOutput));
}

TEST(RomaByobTest, AsyncCallbackExecuteThenDeleteCppBinary) {
  ByobSampleService<> roma_service = GetRomaService(Mode::kModeSandbox);
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

TEST(RomaByobTest, AsyncCallbackExecuteThenCancelCppBinary) {
  ByobSampleService<> roma_service = GetRomaService(Mode::kModeSandbox);
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
  CHECK(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
}

TEST(RomaByobTest, VerifyStdOutStdErrEgressionByChoice) {
  ByobSampleService<> roma_service = GetRomaService(Mode::kModeSandbox);

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusLogBinaryFilename,
               /*enable_log_egress=*/true);

  auto response_and_logs = GetResponseAndLogs(roma_service, code_token);
  EXPECT_THAT(response_and_logs.first.greeting(),
              ::testing::StrEq(kLogUdfOutput));
  EXPECT_THAT(response_and_logs.second,
              ::testing::StrEq("I am a stderr log.\n"));
}

TEST(RomaByobTest, VerifyCodeTokenBasedLoadWorks) {
  ByobSampleService<> roma_service = GetRomaService(Mode::kModeSandbox);
  std::string no_log_code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusLogBinaryFilename);

  std::string log_code_token =
      LoadCodeFromCodeToken(roma_service, no_log_code_token);

  auto response_and_logs = GetResponseAndLogs(roma_service, log_code_token);
  EXPECT_THAT(response_and_logs.first.greeting(),
              ::testing::StrEq(kLogUdfOutput));
  EXPECT_THAT(response_and_logs.second,
              ::testing::StrEq("I am a stderr log.\n"));
}

TEST(RomaByobTest, VerifyRegisterWithAndWithoutLog) {
  ByobSampleService<> roma_service = GetRomaService(Mode::kModeSandbox);
  std::string no_log_code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusLogBinaryFilename);

  std::string log_code_token =
      LoadCodeFromCodeToken(roma_service, no_log_code_token);

  auto response_and_logs = GetResponseAndLogs(roma_service, log_code_token);
  EXPECT_THAT(response_and_logs.first.greeting(),
              ::testing::StrEq(kLogUdfOutput));
  EXPECT_THAT(response_and_logs.second,
              ::testing::StrEq("I am a stderr log.\n"));

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

TEST(RomaByobTest, VerifyHardLinkExecuteWorksAfterDeleteOriginal) {
  ByobSampleService<> roma_service = GetRomaService(Mode::kModeSandbox);
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
              ::testing::StrEq("I am a stderr log.\n"));
  response_and_logs = GetResponseAndLogs(roma_service, log_code_token);
  EXPECT_THAT(response_and_logs.first.greeting(),
              ::testing::StrEq(kLogUdfOutput));
  EXPECT_THAT(response_and_logs.second,
              ::testing::StrEq("I am a stderr log.\n"));
}
}  // namespace
}  // namespace privacy_sandbox::server_common::byob::test
