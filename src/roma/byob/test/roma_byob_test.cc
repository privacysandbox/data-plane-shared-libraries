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

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/byob/udf/sample.pb.h"
#include "src/roma/byob/udf/sample_callback.pb.h"
#include "src/roma/byob/udf/sample_roma_byob_app_service.h"
#include "src/roma/config/function_binding_object_v2.h"

namespace {
using ::google::scp::roma::FunctionBindingObjectV2;
using ::privacy_sandbox::sample_server::roma_app_api::ByobSampleService;
using ::privacy_sandbox::sample_server::roma_app_api::SampleService;
using ::privacy_sandbox::server_common::byob::CallbackReadRequest;
using ::privacy_sandbox::server_common::byob::CallbackReadResponse;
using ::privacy_sandbox::server_common::byob::FUNCTION_CALLBACK;
using ::privacy_sandbox::server_common::byob::FUNCTION_HELLO_WORLD;
using ::privacy_sandbox::server_common::byob::FUNCTION_PRIME_SIEVE;
using ::privacy_sandbox::server_common::byob::FUNCTION_TEN_CALLBACK_INVOCATIONS;
using ::privacy_sandbox::server_common::byob::FunctionType;
using ::privacy_sandbox::server_common::byob::Mode;
using ::privacy_sandbox::server_common::byob::ReadCallbackPayloadRequest;
using ::privacy_sandbox::server_common::byob::SampleRequest;
using ::privacy_sandbox::server_common::byob::SampleResponse;

const std::filesystem::path kUdfPath = "/udf";
const std::filesystem::path kGoLangBinaryFilename = "sample_go_udf";
const std::filesystem::path kCPlusPlusBinaryFilename = "sample_udf";
const std::filesystem::path kCPlusPlusNewBinaryFilename = "new_udf";
const std::filesystem::path kCallbackPayloadReadUdfFilename =
    "callback_payload_read_udf";
constexpr std::string_view kFirstUdfOutput = "Hello, world!";
constexpr std::string_view kNewUdfOutput = "I am a new UDF!";
constexpr std::string_view kGoBinaryOutput = "Hello, world from Go!";

SampleResponse SendRequestAndGetResponse(
    ByobSampleService<>& roma_service, std::string_view code_token,
    FunctionType func_type = FUNCTION_HELLO_WORLD) {
  // Data we are sending to the server.
  SampleRequest bin_request;
  bin_request.set_function(func_type);
  absl::StatusOr<std::unique_ptr<SampleResponse>> response;

  absl::Notification notif;
  CHECK_OK(roma_service.Sample(notif, bin_request, response,
                               /*metadata=*/{}, code_token));
  CHECK(notif.WaitForNotificationWithTimeout(absl::Seconds(1)));
  CHECK_OK(response);
  return *std::move((*response).get());
}

std::string LoadCode(ByobSampleService<>& roma_service,
                     std::filesystem::path file_path) {
  absl::Notification notif;
  absl::Status notif_status;
  absl::StatusOr<std::string> code_id =
      roma_service.Register(file_path, notif, notif_status);
  CHECK_OK(code_id);
  CHECK(notif.WaitForNotificationWithTimeout(absl::Seconds(10)));
  CHECK_OK(notif_status);
  return *std::move(code_id);
}

ByobSampleService<> GetRomaService(Mode mode, int num_workers) {
  privacy_sandbox::server_common::byob::Config<> config = {
      .num_workers = num_workers,
      .roma_container_name = "roma_server",
      .function_bindings = {FunctionBindingObjectV2<>{"example", [](auto&) {}}},
  };
  absl::StatusOr<ByobSampleService<>> sample_interface =
      ByobSampleService<>::Create(config, mode);
  CHECK_OK(sample_interface);
  return std::move(*sample_interface);
}

void ReadCallbackPayload(
    ::google::scp::roma::FunctionBindingPayload<>& wrapper) {
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
}  // namespace

TEST(RomaByobTest, LoadingBinaryInSandboxMode) {
  Mode mode = Mode::kModeSandbox;
  ByobSampleService<> roma_service = GetRomaService(mode, /*num_workers=*/1);

  absl::Notification notif;
  absl::Status notif_status;
  absl::StatusOr<std::string> code_id = roma_service.Register(
      kUdfPath / kCPlusPlusBinaryFilename, notif, notif_status);

  EXPECT_TRUE(code_id.status().ok());
  EXPECT_TRUE(notif.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_TRUE(notif_status.ok());
}

TEST(RomaByobTest, LoadingBinaryInNonSandboxMode) {
  Mode mode = Mode::kModeNoSandbox;
  ByobSampleService<> roma_service = GetRomaService(mode, /*num_workers=*/1);

  absl::Notification notif;
  absl::Status notif_status;
  absl::StatusOr<std::string> code_id = roma_service.Register(
      kUdfPath / kCPlusPlusBinaryFilename, notif, notif_status);

  EXPECT_TRUE(code_id.status().ok());
  EXPECT_TRUE(notif.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_TRUE(notif_status.ok());
}

TEST(RomaByobTest, ExecuteMultipleCppBinariesInSandboxMode) {
  ByobSampleService<> roma_service = GetRomaService(Mode::kModeSandbox,
                                                    /*num_workers=*/2);

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

TEST(RomaByobTest, ExecuteMultipleCppBinariesInNonSandboxMode) {
  ByobSampleService<> roma_service = GetRomaService(Mode::kModeNoSandbox,
                                                    /*num_workers=*/2);

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

TEST(RomaByobTest, ExecuteCppBinaryUsingCallback) {
  ByobSampleService<> roma_service = GetRomaService(Mode::kModeSandbox, 2);

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

  CHECK_OK(roma_service.Sample(callback, bin_request,
                               /*metadata=*/{}, code_token));
  ASSERT_TRUE(notif.WaitForNotificationWithTimeout(absl::Seconds(1)));
  CHECK_OK(bin_response);
  EXPECT_THAT(bin_response->greeting(), kFirstUdfOutput);
}

TEST(RomaByobTest, ExecuteGoLangBinaryInSandboxMode) {
  ByobSampleService<> roma_service =
      GetRomaService(Mode::kModeSandbox, /*num_workers=*/2);

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kGoLangBinaryFilename);

  EXPECT_THAT(SendRequestAndGetResponse(roma_service, code_token).greeting(),
              kGoBinaryOutput);
}

TEST(RomaByobTest, ExecuteCppBinaryWithCallbackInSandboxMode) {
  int64_t elem_size = 5;
  int64_t elem_count = 10;
  ::privacy_sandbox::server_common::byob::Config<> config = {
      .num_workers = 2,
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
  absl::StatusOr<std::unique_ptr<
      privacy_sandbox::server_common::byob::ReadCallbackPayloadResponse>>
      response;
  absl::Notification notif;
  CHECK_OK(roma_service.ReadCallbackPayload(notif, request, response,
                                            /*metadata=*/{}, code_token));

  ASSERT_TRUE(notif.WaitForNotificationWithTimeout(absl::Seconds(1)));
  ASSERT_TRUE(response.ok());
  EXPECT_EQ((*response)->payload_size(), payload_size);
}
