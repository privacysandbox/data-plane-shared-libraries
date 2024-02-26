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

#ifndef CPIO_CLIENT_PROVIDERS_PARAMETER_CLIENT_PROVIDER_MOCK_GCP_MOCK_GCP_PARAMETER_CLIENT_PROVIDER_WITH_OVERRIDES_H_
#define CPIO_CLIENT_PROVIDERS_PARAMETER_CLIENT_PROVIDER_MOCK_GCP_MOCK_GCP_PARAMETER_CLIENT_PROVIDER_WITH_OVERRIDES_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "src/cpio/client_providers/parameter_client_provider/gcp/gcp_parameter_client_provider.h"

namespace google::scp::cpio::client_providers::mock {

class MockGcpParameterClientProviderOverrides
    : public GcpParameterClientProvider {
 public:
  explicit MockGcpParameterClientProviderOverrides(
      core::AsyncExecutorInterface* async_executor,
      core::AsyncExecutorInterface* io_async_executor,
      InstanceClientProviderInterface* instance_client_provider)
      : GcpParameterClientProvider(async_executor, io_async_executor,
                                   instance_client_provider,
                                   ParameterClientOptions()) {}

  std::shared_ptr<cloud::secretmanager::SecretManagerServiceClient>
      secret_manager_mock;

  std::shared_ptr<cloud::secretmanager::SecretManagerServiceClient>
  GetSecretManagerClient() noexcept override {
    if (secret_manager_mock) {
      return secret_manager_mock;
    }
    return nullptr;
  }
};
}  // namespace google::scp::cpio::client_providers::mock

#endif  // CPIO_CLIENT_PROVIDERS_PARAMETER_CLIENT_PROVIDER_MOCK_GCP_MOCK_GCP_PARAMETER_CLIENT_PROVIDER_WITH_OVERRIDES_H_
