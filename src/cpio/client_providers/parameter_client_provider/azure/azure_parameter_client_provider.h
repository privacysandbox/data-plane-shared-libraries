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

#ifndef CPIO_CLIENT_PROVIDERS_PARAMETER_CLIENT_PROVIDER_AZURE_AZURE_PARAMETER_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_PARAMETER_CLIENT_PROVIDER_AZURE_AZURE_PARAMETER_CLIENT_PROVIDER_H_

#include <memory>
#include <string>
#include <utility>

#include "google/cloud/secretmanager/secret_manager_client.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "src/cpio/client_providers/interface/parameter_client_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc ParameterClientProviderInterface
 */
class AzureParameterClientProvider : public ParameterClientProviderInterface {
 public:
  AzureParameterClientProvider() {}

  absl::Status GetParameter(
      core::AsyncContext<
          cmrt::sdk::parameter_service::v1::GetParameterRequest,
          cmrt::sdk::parameter_service::v1::GetParameterResponse>&
          get_parameter_context) noexcept override;

 protected:
  /**
   * @brief Get the default Secret Manager Service Client object.
   *
   * @return std::shared_ptr<cloud::secretmanager::SecretManagerServiceClient>
   */
  virtual std::shared_ptr<cloud::secretmanager::SecretManagerServiceClient>
  GetSecretManagerClient() noexcept;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_PARAMETER_CLIENT_PROVIDER_AZURE_AZURE_PARAMETER_CLIENT_PROVIDER_H_
