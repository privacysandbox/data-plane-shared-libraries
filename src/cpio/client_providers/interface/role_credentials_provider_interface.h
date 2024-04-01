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

#ifndef CPIO_CLIENT_PROVIDERS_INTERFACE_ROLE_CREDENTIALS_PROVIDER_INTERFACE_H_
#define CPIO_CLIENT_PROVIDERS_INTERFACE_ROLE_CREDENTIALS_PROVIDER_INTERFACE_H_

#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/interface/service_interface.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "src/public/cpio/interface/type_def.h"

namespace google::scp::cpio::client_providers {
/// Configurations for RoleCredentialProvider.
struct RoleCredentialsProviderOptions {
  // Location ID for GCP, region code for AWS.
  std::string region;
};

/// Represents the get credentials request object.
struct GetRoleCredentialsRequest {
  // The identity of the account. In AWS, it would be IAM role. In GCP, it would
  // be the service account.
  std::shared_ptr<AccountIdentity> account_identity;
};

/// Represents the get credentials response object.
struct GetRoleCredentialsResponse {
  std::shared_ptr<std::string> access_key_id;
  std::shared_ptr<std::string> access_key_secret;
  std::shared_ptr<std::string> security_token;
};

/// Provides cloud role credentials functionality.
class RoleCredentialsProviderInterface {
 public:
  virtual ~RoleCredentialsProviderInterface() = default;
  /**
   * @brief Gets the role credentials for the given AccountIdentity.
   *
   * @param get_credentials_context The context of the get role credentials
   * operation.
   * @return ExecutionResult The execution result of the operation.
   */
  virtual absl::Status GetRoleCredentials(
      core::AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
          get_role_credentials_context) noexcept = 0;
};

class RoleCredentialsProviderFactory {
 public:
  /**
   * @brief Factory to create RoleCredentialsProviderInterface.
   *
   * @return std::unique_ptr<RoleCredentialsProviderInterface> created
   * RoleCredentialsProviderInterface.
   */
  static absl::StatusOr<std::unique_ptr<RoleCredentialsProviderInterface>>
  Create(
      RoleCredentialsProviderOptions options,
      absl::Nonnull<InstanceClientProviderInterface*> instance_client_provider,
      absl::Nonnull<core::AsyncExecutorInterface*> cpu_async_executor,
      absl::Nonnull<core::AsyncExecutorInterface*> io_async_executor) noexcept;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_INTERFACE_ROLE_CREDENTIALS_PROVIDER_INTERFACE_H_
