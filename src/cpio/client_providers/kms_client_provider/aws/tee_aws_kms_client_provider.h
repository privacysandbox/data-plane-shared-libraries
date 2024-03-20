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

#ifndef CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_AWS_TEE_AWS_KMS_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_AWS_TEE_AWS_KMS_CLIENT_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "src/core/interface/async_context.h"
#include "src/core/interface/credentials_provider_interface.h"
#include "src/cpio/client_providers/interface/kms_client_provider_interface.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc KmsClientProviderInterface
 */
class TeeAwsKmsClientProvider : public KmsClientProviderInterface {
 public:
  static constexpr std::string_view kAwsNitroEnclavesCliPath =
      AWS_NITRO_ENCLAVES_CLI_PATH;

 public:
  /**
   * @brief Constructs a new Aws Enclaves Kms Client Provider.
   *
   * @param credential_provider the credential provider.
   */
  explicit TeeAwsKmsClientProvider(
      absl::Nonnull<RoleCredentialsProviderInterface*> credential_provider)
      : credential_provider_(credential_provider) {}

  TeeAwsKmsClientProvider() = delete;

  absl::Status Decrypt(
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         cmrt::sdk::kms_service::v1::DecryptResponse>&
          decrypt_context) noexcept override;

 protected:
  /**
   * @brief Callback to pass session credentials for decryption.
   *
   * @param create_kms_context the context of created KMS Client.
   * @param get_session_credentials_contexts the context of fetched session
   * credentials.
   * @return core::ExecutionResult the creation results.
   */
  void GetSessionCredentialsCallbackToDecrypt(
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         cmrt::sdk::kms_service::v1::DecryptResponse>&
          decrypt_context,
      core::AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
          get_session_credentials_context) noexcept;

  virtual core::ExecutionResultOr<std::string> DecryptUsingEnclavesKmstoolCli(
      std::string command, std::vector<std::string> args) noexcept;

  /// Credential provider.
  RoleCredentialsProviderInterface* credential_provider_;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_AWS_TEE_AWS_KMS_CLIENT_PROVIDER_H_
