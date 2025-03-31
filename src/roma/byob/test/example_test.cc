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
#include <string>
#include <string_view>

#include "absl/log/initialize.h"
#include "absl/synchronization/notification.h"
#include "src/roma/byob/example/example.pb.h"
#include "src/roma/byob/test/example_roma_byob_app_service.h"
#include "src/roma/byob/utility/utils.h"
#include "src/roma/config/function_binding_object_v2.h"

namespace privacy_sandbox::server_common::byob::example::test {

namespace {
using ::privacy_sandbox::server_common::byob::HasClonePermissionsByobWorker;
using ::privacy_sandbox::server_common::byob::Mode;
using ::privacy_sandbox::server_common::byob::example::ByobEchoService;
using ::privacy_sandbox::server_common::byob::example::EchoRequest;
using ::privacy_sandbox::server_common::byob::example::EchoResponse;
using ::testing::StrEq;

const std::filesystem::path kUdfPath = "/udf";
const std::filesystem::path kGoLangBinaryFilename = "example_go_udf";
const std::filesystem::path kCPlusPlusBinaryFilename = "example_cc_udf";
constexpr absl::Duration kTimeout = absl::Minutes(1);

TEST(RomaByobExampleTest, LoadCppBinaryInGvisorSandbox) {
  auto echo_interface =
      ByobEchoService<>::Create(/*config=*/{}, Mode::kModeGvisorSandbox);
  ASSERT_TRUE(echo_interface.ok()) << echo_interface.status();

  absl::StatusOr<std::string> code_id =
      echo_interface->Register(kUdfPath / kCPlusPlusBinaryFilename,
                               /*num_workers=*/1);

  EXPECT_TRUE(code_id.ok()) << code_id.status();
}

TEST(RomaByobExampleTest, LoadCppBinaryInMinimalSandbox) {
  Mode mode = Mode::kModeMinimalSandbox;
  if (!HasClonePermissionsByobWorker(mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto echo_interface = ByobEchoService<>::Create(/*config=*/{}, mode);
  ASSERT_TRUE(echo_interface.ok()) << echo_interface.status();

  absl::StatusOr<std::string> code_id =
      echo_interface->Register(kUdfPath / kCPlusPlusBinaryFilename,
                               /*num_workers=*/1);

  EXPECT_TRUE(code_id.ok()) << code_id.status();
}

TEST(RomaByobExampleTest, LoadGoBinaryInGvisorSandbox) {
  auto echo_interface =
      ByobEchoService<>::Create(/*config=*/{}, Mode::kModeGvisorSandbox);
  ASSERT_TRUE(echo_interface.ok()) << echo_interface.status();

  absl::StatusOr<std::string> code_id = echo_interface->Register(
      kUdfPath / kGoLangBinaryFilename, /*num_workers=*/1);

  EXPECT_TRUE(code_id.ok()) << code_id.status();
}

TEST(RomaByobExampleTest, LoadGoBinaryInMinimalSandbox) {
  Mode mode = Mode::kModeMinimalSandbox;
  if (!HasClonePermissionsByobWorker(mode)) {
    GTEST_SKIP() << "HasClonePermissionsByobWorker check returned false";
  }
  auto echo_interface = ByobEchoService<>::Create(/*config=*/{}, mode);
  ASSERT_TRUE(echo_interface.ok()) << echo_interface.status();

  absl::StatusOr<std::string> code_id = echo_interface->Register(
      kUdfPath / kGoLangBinaryFilename, /*num_workers=*/1);

  EXPECT_TRUE(code_id.ok()) << code_id.status();
}

TEST(RomaByobExampleTest, NotifProcessRequestCppBinary) {
  auto echo_interface =
      ByobEchoService<>::Create(/*config=*/{}, Mode::kModeGvisorSandbox);
  ASSERT_TRUE(echo_interface.ok()) << echo_interface.status();

  auto code_token = echo_interface->Register(
      kUdfPath / kCPlusPlusBinaryFilename, /*num_workers=*/2);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  EchoRequest request;
  const std::string message = "I am a test Cpp message.";
  request.set_message(message);
  absl::StatusOr<EchoResponse> response;
  absl::Notification notif;

  ASSERT_TRUE(echo_interface
                  ->Echo(notif, std::move(request), response,
                         /*metadata=*/{}, *code_token, kTimeout)
                  .ok());

  ASSERT_TRUE(notif.WaitForNotificationWithTimeout(kTimeout));
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_THAT(response->message(), StrEq(message));
}

TEST(RomaByobExampleTest, AsyncCallbackProcessRequestCppBinary) {
  auto echo_interface =
      ByobEchoService<>::Create(/*config=*/{}, Mode::kModeGvisorSandbox);
  ASSERT_TRUE(echo_interface.ok()) << echo_interface.status();

  auto code_token = echo_interface->Register(
      kUdfPath / kCPlusPlusBinaryFilename, /*num_workers=*/2);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  const std::string message = "I am a test Cpp message.";
  EchoRequest bin_request;
  bin_request.set_message(message);
  absl::Notification notif;
  absl::StatusOr<EchoResponse> bin_response;
  auto callback = [&notif, &bin_response](
                      absl::StatusOr<EchoResponse> resp,
                      absl::StatusOr<std::string_view> /*logs*/,
                      ProcessRequestMetrics /*metrics*/) {
    bin_response = std::move(resp);
    notif.Notify();
  };

  ASSERT_TRUE(echo_interface
                  ->Echo(callback, std::move(bin_request),
                         /*metadata=*/{}, *code_token, kTimeout)
                  .ok());

  ASSERT_TRUE(notif.WaitForNotificationWithTimeout(kTimeout));
  ASSERT_TRUE(bin_response.ok()) << bin_response.status();
  EXPECT_THAT(bin_response->message(), StrEq(message));
}

TEST(RomaByobExampleTest, NotifProcessRequestGoBinary) {
  auto echo_interface = ByobEchoService<>::Create(/*config=*/
                                                  {
                                                      .lib_mounts = "",
                                                  },
                                                  Mode::kModeGvisorSandbox);
  ASSERT_TRUE(echo_interface.ok()) << echo_interface.status();

  auto code_token = echo_interface->Register(kUdfPath / kGoLangBinaryFilename,
                                             /*num_workers=*/2);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  const std::string message = "I am a test Go binary message.";
  EchoRequest request;
  request.set_message(message);
  absl::StatusOr<EchoResponse> response;
  absl::Notification notif;

  ASSERT_TRUE(echo_interface
                  ->Echo(notif, std::move(request), response,
                         /*metadata=*/{}, *code_token, kTimeout)
                  .ok());

  ASSERT_TRUE(notif.WaitForNotificationWithTimeout(kTimeout));
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_THAT(response->message(), StrEq(message));
}

TEST(RomaByobExampleTest, AsyncCallbackProcessRequestGoBinary) {
  auto echo_interface = ByobEchoService<>::Create(/*config=*/
                                                  {
                                                      .lib_mounts = "",
                                                  },
                                                  Mode::kModeGvisorSandbox);
  ASSERT_TRUE(echo_interface.ok()) << echo_interface.status();

  auto code_token = echo_interface->Register(kUdfPath / kGoLangBinaryFilename,
                                             /*num_workers=*/2);
  ASSERT_TRUE(code_token.ok()) << code_token.status();

  const std::string message = "I am a test Go binary message.";
  EchoRequest bin_request;
  bin_request.set_message(message);
  absl::Notification notif;
  absl::StatusOr<EchoResponse> bin_response;
  auto callback = [&notif, &bin_response](
                      absl::StatusOr<EchoResponse> resp,
                      absl::StatusOr<std::string_view> /*logs*/,
                      ProcessRequestMetrics /*metrics*/) {
    bin_response = std::move(resp);
    notif.Notify();
  };

  ASSERT_TRUE(echo_interface
                  ->Echo(callback, std::move(bin_request),
                         /*metadata=*/{}, *code_token, kTimeout)
                  .ok());

  ASSERT_TRUE(notif.WaitForNotificationWithTimeout(kTimeout));
  ASSERT_TRUE(bin_response.ok()) << bin_response.status();
  EXPECT_THAT(bin_response->message(), StrEq(message));
}

}  // namespace
}  // namespace privacy_sandbox::server_common::byob::example::test

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
