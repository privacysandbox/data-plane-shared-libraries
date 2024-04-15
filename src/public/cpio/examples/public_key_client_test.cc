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
#include "src/core/interface/async_context.h"
#include "src/public/core/interface/errors.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/cpio.h"
#include "src/public/cpio/interface/public_key_client/public_key_client_interface.h"
#include "src/public/cpio/interface/public_key_client/type_def.h"
#include "src/public/cpio/interface/type_def.h"
#include "src/public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

using google::cmrt::sdk::public_key_service::v1::ListPublicKeysRequest;
using google::cmrt::sdk::public_key_service::v1::ListPublicKeysResponse;
using google::cmrt::sdk::public_key_service::v1::PublicKey;
using google::scp::core::ExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::LogOption;
using google::scp::cpio::PublicKeyClientFactory;
using google::scp::cpio::PublicKeyClientOptions;

namespace {
constexpr std::string_view kPublicKeyEndpoint = "https://test.publickey.com";
}

int main(int argc, char* argv[]) {
  CpioOptions cpio_options;
  cpio_options.log_option = LogOption::kConsoleLog;
  auto result = Cpio::InitCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to initialize CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }

  PublicKeyClientOptions public_key_client_options;
  public_key_client_options.endpoints.emplace_back(kPublicKeyEndpoint);

  auto public_key_client =
      PublicKeyClientFactory::Create(std::move(public_key_client_options));
  std::cout << "Run public key client successfully!" << std::endl;

  ListPublicKeysRequest request;
  absl::Notification finished;
  if (absl::Status error = public_key_client->ListPublicKeys(
          std::move(request),
          [&](const ExecutionResult result, ListPublicKeysResponse response) {
            if (!result.Successful()) {
              std::cout << "ListPublicKeys failed: "
                        << GetErrorMessage(result.status_code) << std::endl;
            } else {
              std::cout << "ListPublicKeys succeeded. The key count is: "
                        << response.public_keys_size() << std::endl;
            }
            finished.Notify();
          });
      !error.ok()) {
    std::cout << "ListPublicKeys failed immediately: " << error << std::endl;
  }
  finished.WaitForNotificationWithTimeout(absl::Seconds(100));
  result = Cpio::ShutdownCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to shutdown CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }
}
