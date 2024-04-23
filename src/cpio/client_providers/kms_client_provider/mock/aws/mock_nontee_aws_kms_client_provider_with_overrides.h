/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_MOCK_AWS_MOCK_NONTEE_AWS_KMS_CLIENT_PROVIDER_WITH_OVERRIDES_H_
#define CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_MOCK_AWS_MOCK_NONTEE_AWS_KMS_CLIENT_PROVIDER_WITH_OVERRIDES_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <aws/kms/KMSClient.h>

#include "src/cpio/client_providers/kms_client_provider/aws/nontee_aws_kms_client_provider.h"

#include "mock_kms_client.h"

namespace google::scp::cpio::client_providers::mock {
class MockNonteeAwsKmsClientProviderWithOverrides
    : public NonteeAwsKmsClientProvider {
 public:
  MockNonteeAwsKmsClientProviderWithOverrides(
      RoleCredentialsProviderInterface* role_credential_provider,
      std::shared_ptr<Aws::KMS::KMSClient> mock_kms_client,
      core::AsyncExecutorInterface* io_async_executor)
      : NonteeAwsKmsClientProvider(role_credential_provider,
                                   io_async_executor) {
    kms_client_ = std::move(mock_kms_client);
  }

  std::shared_ptr<Aws::KMS::KMSClient> GetKmsClient(
      const std::shared_ptr<Aws::Auth::AWSCredentials>& aws_credentials,
      const std::shared_ptr<std::string>& kms_region) noexcept override {
    return kms_client_;
  }

  std::shared_ptr<Aws::KMS::KMSClient> kms_client_;
};
}  // namespace google::scp::cpio::client_providers::mock

#endif  // CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_MOCK_AWS_MOCK_NONTEE_AWS_KMS_CLIENT_PROVIDER_WITH_OVERRIDES_H_
