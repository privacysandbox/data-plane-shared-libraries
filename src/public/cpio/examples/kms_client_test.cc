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
#include "absl/strings/strip.h"
#include "absl/synchronization/notification.h"
#include "src/core/interface/async_context.h"
#include "src/public/core/interface/errors.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/cpio.h"
#include "src/public/cpio/interface/kms_client/kms_client_interface.h"
#include "src/public/cpio/interface/kms_client/type_def.h"
#include "src/public/cpio/interface/type_def.h"
#include "src/public/cpio/proto/kms_service/v1/kms_service.pb.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::KmsClientFactory;
using google::scp::cpio::KmsClientInterface;
using google::scp::cpio::KmsClientOptions;
using google::scp::cpio::LogOption;

namespace {
constexpr std::string_view kCiphertext = "test_ciphertext";
constexpr std::string_view kKeyResourceName = "test_key_resource_name";
}  // namespace

int main(int argc, char* argv[]) {
  CpioOptions cpio_options;
  cpio_options.log_option = LogOption::kConsoleLog;
  auto result = Cpio::InitCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to initialize CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }

  KmsClientOptions kms_client_options;

  auto kms_client = KmsClientFactory::Create(std::move(kms_client_options));
  if (const absl::Status error = kms_client->Init(); !error.ok()) {
    std::cout << "Cannot init kms client!" << error << std::endl;
    return 0;
  }
  std::cout << "Run kms client successfully!" << std::endl;

  auto request = std::make_shared<DecryptRequest>();
  request->set_ciphertext(kCiphertext);
  request->set_key_resource_name(kKeyResourceName);

  absl::Notification finished;
  AsyncContext<DecryptRequest, DecryptResponse> decrypt_context(
      std::move(request), [&finished](auto& context) {
        if (!context.result.Successful()) {
          std::cout << "Decrypt failed: "
                    << GetErrorMessage(context.result.status_code) << std::endl;
        } else {
          std::cout << "Decrypt succeeded." << std::endl;
        }
        finished.Notify();
      });
  if (absl::Status error = kms_client->Decrypt(decrypt_context); !error.ok()) {
    std::cout << "Decrypt failed immediately: " << error << std::endl;
  }
  finished.WaitForNotificationWithTimeout(absl::Seconds(100));
  result = Cpio::ShutdownCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to shutdown CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }
}
