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

#include "absl/synchronization/notification.h"
#include "src/roma/byob/example/example.pb.h"
#include "src/roma/byob/test/example_roma_byob_app_service.h"
#include "src/roma/config/function_binding_object_v2.h"

namespace privacy_sandbox::server_common::byob::example::test {

namespace {
using ::privacy_sandbox::server_common::byob::Mode;
using ::privacy_sandbox::server_common::byob::example::ByobEchoService;
using ::privacy_sandbox::server_common::byob::example::EchoRequest;
using ::privacy_sandbox::server_common::byob::example::EchoResponse;
using ::testing::StrEq;

const std::filesystem::path kUdfPath = "/udf";
const std::filesystem::path kGoLangBinaryFilename = "example_go_udf";
const std::filesystem::path kCPlusPlusBinaryFilename = "example_cc_udf";

std::string LoadCode(ByobEchoService<>& roma_service,
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

ByobEchoService<> GetRomaService(Mode mode, int num_workers) {
  privacy_sandbox::server_common::byob::Config<> config = {
      .num_workers = num_workers,
      .roma_container_name = "roma_server",
  };
  absl::StatusOr<ByobEchoService<>> echo_interface =
      ByobEchoService<>::Create(config, mode);
  CHECK_OK(echo_interface);
  return std::move(*echo_interface);
}

TEST(RomaByobExampleTest, LoadCppBinaryInSandboxMode) {
  Mode mode = Mode::kModeSandbox;
  ByobEchoService<> roma_service = GetRomaService(mode, /*num_workers=*/1);

  absl::Notification notif;
  absl::Status notif_status;
  absl::StatusOr<std::string> code_id = roma_service.Register(
      kUdfPath / kCPlusPlusBinaryFilename, notif, notif_status);

  EXPECT_TRUE(code_id.ok());
  EXPECT_TRUE(notif.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_TRUE(notif_status.ok());
}

TEST(RomaByobExampleTest, LoadCppBinaryInNonSandboxMode) {
  Mode mode = Mode::kModeNoSandbox;
  ByobEchoService<> roma_service = GetRomaService(mode, /*num_workers=*/1);
  absl::Notification notif;
  absl::Status notif_status;

  absl::StatusOr<std::string> code_id = roma_service.Register(
      kUdfPath / kCPlusPlusBinaryFilename, notif, notif_status);

  EXPECT_TRUE(code_id.ok());
  EXPECT_TRUE(notif.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_TRUE(notif_status.ok());
}

TEST(RomaByobExampleTest, LoadGoBinaryInSandboxMode) {
  Mode mode = Mode::kModeSandbox;
  ByobEchoService<> roma_service = GetRomaService(mode, /*num_workers=*/1);
  absl::Notification notif;
  absl::Status notif_status;

  absl::StatusOr<std::string> code_id = roma_service.Register(
      kUdfPath / kGoLangBinaryFilename, notif, notif_status);

  EXPECT_TRUE(code_id.ok());
  EXPECT_TRUE(notif.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_TRUE(notif_status.ok());
}

TEST(RomaByobExampleTest, LoadGoBinaryInNonSandboxMode) {
  Mode mode = Mode::kModeNoSandbox;
  ByobEchoService<> roma_service = GetRomaService(mode, /*num_workers=*/1);
  absl::Notification notif;
  absl::Status notif_status;

  absl::StatusOr<std::string> code_id = roma_service.Register(
      kUdfPath / kGoLangBinaryFilename, notif, notif_status);

  EXPECT_TRUE(code_id.ok());
  EXPECT_TRUE(notif.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_TRUE(notif_status.ok());
}

TEST(RomaByobExampleTest, NotifExecuteCppBinary) {
  ByobEchoService<> roma_service = GetRomaService(Mode::kModeSandbox, 2);
  const std::string message = "I am a test Cpp message.";
  const std::string code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusBinaryFilename);
  EchoRequest request;
  request.set_message(message);
  absl::StatusOr<std::unique_ptr<EchoResponse>> response;
  absl::Notification notif;

  CHECK_OK(roma_service.Echo(notif, request, response,
                             /*metadata=*/{}, code_token));

  CHECK(notif.WaitForNotificationWithTimeout(absl::Seconds(1)));
  CHECK_OK(response);
  CHECK(*response != nullptr);
  EXPECT_THAT((*response)->message(), StrEq(message));
}

TEST(RomaByobExampleTest, AsyncCallbackExecuteCppBinary) {
  ByobEchoService<> roma_service = GetRomaService(Mode::kModeSandbox, 2);
  const std::string message = "I am a test Cpp message.";
  const std::string code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusBinaryFilename);
  EchoRequest bin_request;
  bin_request.set_message(message);
  absl::Notification notif;
  absl::StatusOr<EchoResponse> bin_response;
  auto callback = [&notif, &bin_response](absl::StatusOr<EchoResponse> resp) {
    bin_response = std::move(resp);
    notif.Notify();
  };

  CHECK_OK(roma_service.Echo(callback, bin_request,
                             /*metadata=*/{}, code_token));

  ASSERT_TRUE(notif.WaitForNotificationWithTimeout(absl::Seconds(1)));
  CHECK_OK(bin_response);
  EXPECT_THAT(bin_response->message(), StrEq(message));
}

TEST(RomaByobExampleTest, NotifExecuteGoBinary) {
  ByobEchoService<> roma_service = GetRomaService(Mode::kModeSandbox, 2);
  const std::string message = "I am a test Go binary message.";
  const std::string code_token =
      LoadCode(roma_service, kUdfPath / kGoLangBinaryFilename);
  EchoRequest request;
  request.set_message(message);
  absl::StatusOr<std::unique_ptr<EchoResponse>> response;
  absl::Notification notif;

  CHECK_OK(roma_service.Echo(notif, request, response,
                             /*metadata=*/{}, code_token));

  CHECK(notif.WaitForNotificationWithTimeout(absl::Seconds(1)));
  CHECK_OK(response);
  CHECK(*response != nullptr);
  EXPECT_THAT((*response)->message(), StrEq(message));
}

TEST(RomaByobExampleTest, AsyncCallbackExecuteGoBinary) {
  ByobEchoService<> roma_service = GetRomaService(Mode::kModeSandbox, 2);
  const std::string message = "I am a test Go binary message.";
  const std::string code_token =
      LoadCode(roma_service, kUdfPath / kGoLangBinaryFilename);
  EchoRequest bin_request;
  bin_request.set_message(message);
  absl::Notification notif;
  absl::StatusOr<EchoResponse> bin_response;
  auto callback = [&notif, &bin_response](absl::StatusOr<EchoResponse> resp) {
    bin_response = std::move(resp);
    notif.Notify();
  };

  CHECK_OK(roma_service.Echo(callback, bin_request,
                             /*metadata=*/{}, code_token));

  ASSERT_TRUE(notif.WaitForNotificationWithTimeout(absl::Seconds(1)));
  CHECK_OK(bin_response);
  EXPECT_THAT(bin_response->message(), StrEq(message));
}

}  // namespace
}  // namespace privacy_sandbox::server_common::byob::example::test
