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

#ifndef CPIO_CLIENT_PROVIDERS_INTERFACE_KMS_CLIENT_PROVIDER_INTERFACE_H_
#define CPIO_CLIENT_PROVIDERS_INTERFACE_KMS_CLIENT_PROVIDER_INTERFACE_H_

#include <memory>
#include <string>

#include "absl/base/nullability.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/service_interface.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/kms_client/type_def.h"
#include "src/public/cpio/interface/type_def.h"
#include "src/public/cpio/proto/kms_service/v1/kms_service.pb.h"

namespace google::scp::cpio::client_providers {

/**
 * @brief Interface responsible for fetching KMS Aead.
 */
class KmsClientProviderInterface {
 public:
  virtual ~KmsClientProviderInterface() = default;
  /**
   * @brief Decrypt cipher text using KMS Client.
   *
   * @param decrypt_context decrypt_context of the operation.
   * @return core::ExecutionResult execution result.
   */
  virtual absl::Status Decrypt(
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         cmrt::sdk::kms_service::v1::DecryptResponse>&
          decrypt_context) noexcept = 0;
};

class KmsClientProviderFactory {
 public:
  /**
   * @brief Factory to create KmsClientProvider.
   *
   * @return std::unique_ptr<KmsClientProviderInterface> created
   * KmsClientProvider.
   */
  static absl::Nonnull<std::unique_ptr<KmsClientProviderInterface>> Create(
      absl::Nonnull<RoleCredentialsProviderInterface*>
          role_credentials_provider,
      absl::Nonnull<core::AsyncExecutorInterface*> io_async_executor) noexcept;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_INTERFACE_KMS_CLIENT_PROVIDER_INTERFACE_H_
