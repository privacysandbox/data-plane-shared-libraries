// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/synchronization/notification.h"
#include "src/public/core/interface/errors.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/private_key_client/private_key_client_interface.h"
#include "src/public/cpio/interface/private_key_client/type_def.h"
#include "src/public/cpio/interface/type_def.h"
#include "src/public/cpio/test/global_cpio/test_lib_cpio.h"

using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse;
using google::scp::core::ExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::LogOption;
using google::scp::cpio::PrivateKeyClientFactory;
using google::scp::cpio::PrivateKeyClientInterface;
using google::scp::cpio::PrivateKeyClientOptions;
using google::scp::cpio::PrivateKeyVendingEndpoint;
using google::scp::cpio::TestCpioOptions;
using google::scp::cpio::TestLibCpio;

namespace {
constexpr std::string_view kPrivateKeyEndpoint1 =
    "https://test.privatekey1.com";
constexpr std::string_view kPrivateKeyEndpoint2 =
    "https://test.privatekey2.com";
constexpr std::string_view kIamRole1 =
    "arn:aws:iam::1234:role/test_assume_role_1";
constexpr std::string_view kIamRole2 =
    "arn:aws:iam::1234:role/test_assume_role_2";
constexpr std::string_view kServiceRegion = "us-east-1";
constexpr std::string_view kKeyId1 = "key-id";
}  // namespace

int main(int argc, char* argv[]) {
  TestCpioOptions cpio_options{
      .options = {.log_option = LogOption::kConsoleLog,
                  .region = std::string{kServiceRegion}}};
  TestLibCpio::InitCpio(cpio_options);
  PrivateKeyClientOptions private_key_client_options;
  PrivateKeyVendingEndpoint primary_endpoint;
  primary_endpoint.account_identity = kIamRole1;
  primary_endpoint.service_region = kServiceRegion;
  primary_endpoint.private_key_vending_service_endpoint = kPrivateKeyEndpoint1;
  private_key_client_options.primary_private_key_vending_endpoint =
      primary_endpoint;

  PrivateKeyVendingEndpoint secondary_endpoint;
  secondary_endpoint.account_identity = kIamRole2;
  secondary_endpoint.service_region = kServiceRegion;
  secondary_endpoint.private_key_vending_service_endpoint =
      kPrivateKeyEndpoint2;
  private_key_client_options.secondary_private_key_vending_endpoints
      .emplace_back(secondary_endpoint);

  auto private_key_client =
      PrivateKeyClientFactory::Create(std::move(private_key_client_options));
  if (const absl::Status error = private_key_client->Init(); !error.ok()) {
    std::cout << "Cannot init private key client!" << error << std::endl;
    return 0;
  }
  std::cout << "Run private key client successfully!" << std::endl;

  ListPrivateKeysRequest request;
  request.add_key_ids(kKeyId1);
  absl::Notification finished;
  if (absl::Status error = private_key_client->ListPrivateKeys(
          std::move(request),
          [&](const ExecutionResult result, ListPrivateKeysResponse response) {
            if (!result.Successful()) {
              std::cout << "ListPrivateKeys failed: "
                        << GetErrorMessage(result.status_code) << std::endl;
            } else {
              std::cout << "ListPrivateKeys succeeded." << std::endl;
            }
            finished.Notify();
          });
      !error.ok()) {
    std::cout << "ListPrivateKeys failed immediately: " << error << std::endl;
  }
  finished.WaitForNotificationWithTimeout(absl::Seconds(100));
  TestLibCpio::ShutdownCpio(cpio_options);
}
