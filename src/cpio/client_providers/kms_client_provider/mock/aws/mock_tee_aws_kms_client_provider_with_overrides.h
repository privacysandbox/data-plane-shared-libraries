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

#ifndef CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_MOCK_AWS_MOCK_TEE_AWS_KMS_CLIENT_PROVIDER_WITH_OVERRIDES_H_
#define CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_MOCK_AWS_MOCK_TEE_AWS_KMS_CLIENT_PROVIDER_WITH_OVERRIDES_H_

#include <memory>
#include <string>
#include <vector>

#include "src/cpio/client_providers/kms_client_provider/aws/tee_aws_kms_client_provider.h"

namespace google::scp::cpio::client_providers::mock {
class MockTeeAwsKmsClientProviderWithOverrides
    : public TeeAwsKmsClientProvider {
 public:
  MockTeeAwsKmsClientProviderWithOverrides(
      RoleCredentialsProviderInterface* credential_provider)
      : TeeAwsKmsClientProvider(credential_provider) {}

  core::ExecutionResultOr<std::string> DecryptUsingEnclavesKmstoolCli(
      std::string command, std::vector<std::string> args) noexcept override {
    return returned_plaintext;
  }

  std::string returned_plaintext;
};
}  // namespace google::scp::cpio::client_providers::mock

#endif  // CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_MOCK_AWS_MOCK_TEE_AWS_KMS_CLIENT_PROVIDER_WITH_OVERRIDES_H_
