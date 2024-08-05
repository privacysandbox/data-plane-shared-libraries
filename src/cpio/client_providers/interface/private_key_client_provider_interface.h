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

#ifndef CPIO_CLIENT_PROVIDERS_INTERFACE_PRIVATE_KEY_CLIENT_PROVIDER_INTERFACE_H_
#define CPIO_CLIENT_PROVIDERS_INTERFACE_PRIVATE_KEY_CLIENT_PROVIDER_INTERFACE_H_

#include <memory>
#include <string>

#include "absl/base/nullability.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/http_client_interface.h"
#include "src/core/interface/service_interface.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/private_key_client/type_def.h"
#include "src/public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Interface responsible for fetching private keys.
 */
class PrivateKeyClientProviderInterface {
 public:
  virtual ~PrivateKeyClientProviderInterface() = default;
  /**
   * @brief Fetches list of private keys by ids.
   *
   * @param context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual absl::Status ListPrivateKeys(
      core::AsyncContext<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest,
          cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>&
          context) noexcept = 0;
};

class PrivateKeyClientProviderFactory {
 public:
  /**
   * @brief Factory to create PrivateKeyClientProvider.
   *
   * @return std::unique_ptr<PrivateKeyClientProviderInterface> created
   * PrivateKeyClientProvider.
   */
  static absl::Nonnull<std::unique_ptr<PrivateKeyClientProviderInterface>>
  Create(PrivateKeyClientOptions options,
         absl::Nonnull<core::HttpClientInterface*> http_client,
         absl::Nonnull<RoleCredentialsProviderInterface*>
             role_credentials_provider,
         absl::Nonnull<AuthTokenProviderInterface*> auth_token_provider,
         absl::Nonnull<core::AsyncExecutorInterface*> io_async_executor);
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_INTERFACE_PRIVATE_KEY_CLIENT_PROVIDER_INTERFACE_H_
