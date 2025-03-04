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

#include "azure_role_credentials_provider.h"

#include <memory>

#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;

namespace google::scp::cpio::client_providers {
absl::Status AzureRoleCredentialsProvider::GetRoleCredentials(
    core::AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
        get_credentials_context) noexcept {
  return absl::UnknownError("");
}

absl::StatusOr<std::unique_ptr<RoleCredentialsProviderInterface>>
RoleCredentialsProviderFactory::Create(
    RoleCredentialsProviderOptions /*options*/,
    absl::Nonnull<
        InstanceClientProviderInterface*> /*instance_client_provider*/,
    absl::Nonnull<core::AsyncExecutorInterface*> /*cpu_async_executor*/,
    absl::Nonnull<
        core::AsyncExecutorInterface*> /*io_async_executor*/) noexcept {
  return std::make_unique<AzureRoleCredentialsProvider>();
}
}  // namespace google::scp::cpio::client_providers
