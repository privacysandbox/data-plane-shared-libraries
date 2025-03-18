/*
 * Portions Copyright (c) Microsoft Corporation
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

#ifndef CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_AZURE_AZURE_KMS_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_AZURE_AZURE_KMS_CLIENT_PROVIDER_H_

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "src/azure/attestation/src/attestation.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/interface/kms_client_provider_interface.h"
#include "src/public/core/interface/execution_result.h"

#include "azure_kms_client_provider_utils.h"

namespace google::scp::cpio::client_providers {

/*! @copydoc KmsClientProviderInterface
 */
class AzureKmsClientProvider : public KmsClientProviderInterface {
 public:
  explicit AzureKmsClientProvider(
      core::HttpClientInterface* http_client,
      AuthTokenProviderInterface* auth_token_provider)
      : http_client_(http_client),
        auth_token_provider_(auth_token_provider),
        unwrap_url_() {}

  absl::Status Decrypt(
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         cmrt::sdk::kms_service::v1::DecryptResponse>&
          decrypt_context) noexcept override;

 private:
  /**
   * @brief Callback to pass token for decryption.
   *
   * @param create_kms_context the context of created KMS Client.
   * @param get_token_context the context of fetched auth token
   * credentials.
   * @return core::ExecutionResult the creation results.
   */
  void GetSessionCredentialsCallbackToDecrypt(
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         cmrt::sdk::kms_service::v1::DecryptResponse>&
          decrypt_context,
      core::AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
          get_token_context) noexcept;

  /**
   * @brief Is called when the decrypt operation
   * is completed.
   *
   * @param decrypt_context The context of the decrypt operation.
   * @param http_client_context http client operation context.
   */
  void OnDecryptCallback(
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         cmrt::sdk::kms_service::v1::DecryptResponse>&
          decrypt_context,
      std::shared_ptr<EvpPkeyWrapper> ephemeral_private_key,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_client_context) noexcept;

  absl::StatusOr<std::pair<std::shared_ptr<EvpPkeyWrapper>,
                           std::shared_ptr<EvpPkeyWrapper>>>
  GenerateWrappingKeyPair() noexcept;

  std::optional<azure::attestation::AttestationReport> FetchSnpAttestation(
      const std::string report_data = "") noexcept;

  core::HttpClientInterface* http_client_;
  // Auth token provider.
  AuthTokenProviderInterface* auth_token_provider_;

  std::string unwrap_url_;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_AZURE_AZURE_KMS_CLIENT_PROVIDER_H_
