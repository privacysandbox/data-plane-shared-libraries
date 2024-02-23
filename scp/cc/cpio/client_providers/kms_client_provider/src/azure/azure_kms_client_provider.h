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

#ifndef CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_SRC_AZURE_AZURE_KMS_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_SRC_AZURE_AZURE_KMS_CLIENT_PROVIDER_H_

#include <memory>
#include <string>

#include <tink/aead.h>

#include "core/interface/async_context.h"
#include "cpio/client_providers/interface/kms_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "azure/attestation/json_attestation_report.h"

namespace google::scp::cpio::client_providers {

/*! @copydoc KmsClientProviderInterface
 */
class AzureKmsClientProvider : public KmsClientProviderInterface {
 public:

  explicit AzureKmsClientProvider(
      const std::shared_ptr<core::HttpClientInterface>&
          http_client)
      : http_client_(http_client) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult Decrypt(
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         cmrt::sdk::kms_service::v1::DecryptResponse>&
          decrypt_context) noexcept override;

 private:

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
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_client_context) noexcept;

  std::shared_ptr<core::HttpClientInterface> http_client_;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_SRC_AZURE_AZURE_KMS_CLIENT_PROVIDER_H_
