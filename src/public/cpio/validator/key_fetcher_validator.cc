// Copyright 2023 Google LLC
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

#include "src/public/cpio/validator/key_fetcher_validator.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/log/log.h"
#include "absl/synchronization/notification.h"
#include "src/core/interface/async_context.h"
#include "src/public/core/interface/errors.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/validator/proto/validator_config.pb.h"

namespace google::scp::cpio::validator {

namespace {
using google::scp::core::AsyncContext;
using google::scp::cpio::client_providers::AuthTokenProviderInterface;
using google::scp::cpio::client_providers::PrivateKeyFetchingRequest;
using google::scp::cpio::client_providers::PrivateKeyFetchingResponse;
using google::scp::cpio::client_providers::RoleCredentialsProviderInterface;
using google::scp::cpio::validator::proto::FetchPrivateKeyConfig;

std::shared_ptr<PrivateKeyFetchingRequest> CreatePKRequest(
    const FetchPrivateKeyConfig& key_fetcher_config) {
  auto key_fetcher_request = std::make_shared<PrivateKeyFetchingRequest>();
  key_fetcher_request->key_id =
      std::make_shared<std::string>(key_fetcher_config.key_id());
  auto endpoint =
      std::make_shared<PrivateKeyVendingEndpoint>(PrivateKeyVendingEndpoint{});
  endpoint->private_key_vending_service_endpoint =
      key_fetcher_config.private_key_vending_service_endpoint();
  endpoint->service_region = key_fetcher_config.service_region();
  endpoint->account_identity = key_fetcher_config.account_identity();
  key_fetcher_request->key_vending_endpoint = std::move(endpoint);
  return key_fetcher_request;
}

}  // namespace

void RunFetchPrivateKeyValidator(
    client_providers::CpioProviderInterface& cpio, std::string_view name,
    const FetchPrivateKeyConfig& key_fetcher_config) {
  if (key_fetcher_config.key_id().empty()) {
    std::cout << "[ FAILURE ] " << name << " No key id provided." << std::endl;
    return;
  }

  if (key_fetcher_config.private_key_vending_service_endpoint().empty()) {
    std::cout << "[ FAILURE ] " << name
              << " No private key vending service endpoint provided."
              << std::endl;
    return;
  }

  if (key_fetcher_config.service_region().empty()) {
    std::cout << "[ FAILURE ] " << name << " No service region provided."
              << std::endl;
    return;
  }

  if (key_fetcher_config.account_identity().empty()) {
    std::cout << "[ FAILURE ] " << name << " No account identity provided."
              << std::endl;
    return;
  }
  auto role_credentials_provider = cpio.GetRoleCredentialsProvider();
  if (!role_credentials_provider.ok()) {
    std::cout << "[ FAILURE ] " << role_credentials_provider.status()
              << std::endl;
    return;
  }
  auto key_fetcher =
      google::scp::cpio::client_providers::PrivateKeyFetcherProviderFactory::
          Create(&cpio.GetHttp1Client(), *role_credentials_provider,
                 &cpio.GetAuthTokenProvider());
  if (google::scp::core::ExecutionResult result = key_fetcher->Init();
      !result.Successful()) {
    std::cout << "[ FAILURE ] " << name << " "
              << core::errors::GetErrorMessage(result.status_code) << std::endl
              << std::endl;
    return;
  }
  if (google::scp::core::ExecutionResult result = key_fetcher->Run();
      !result.Successful()) {
    std::cout << "[ FAILURE ] " << name << " "
              << core::errors::GetErrorMessage(result.status_code) << std::endl
              << std::endl;
    return;
  }

  // FetchPrivateKey.
  absl::Notification finished;
  google::scp::core::ExecutionResult result;

  auto key_fetcher_request = CreatePKRequest(key_fetcher_config);

  AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>
      key_fetcher_context(std::move(key_fetcher_request),
                          [&result, &finished, &name](auto& context) {
                            result = context.result;
                            if (result.Successful()) {
                              std::cout << "[ SUCCESS ] " << name << " "
                                        << std::endl;
                            }
                            finished.Notify();
                          });
  if (auto key_fetcher_result =
          key_fetcher->FetchPrivateKey(key_fetcher_context);
      !key_fetcher_result.Successful()) {
    std::cout << "[ FAILURE ] " << name << " "
              << core::errors::GetErrorMessage(key_fetcher_result.status_code)
              << std::endl;
  }
  finished.WaitForNotification();
  if (!result.Successful()) {
    std::cout << "[ FAILURE ] " << name << " "
              << core::errors::GetErrorMessage(result.status_code) << std::endl;
  }
  if (auto result = key_fetcher->Stop(); !result.Successful()) {
    std::cout << " [ FAILURE ] " << name << " "
              << core::errors::GetErrorMessage(result.status_code) << std::endl;
  }
}

}  // namespace google::scp::cpio::validator
