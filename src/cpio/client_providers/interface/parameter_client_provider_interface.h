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

#ifndef CPIO_CLIENT_PROVIDERS_INTERFACE_PARAMETER_CLIENT_PROVIDER_INTERFACE_H_
#define CPIO_CLIENT_PROVIDERS_INTERFACE_PARAMETER_CLIENT_PROVIDER_INTERFACE_H_

#include <memory>
#include <string>

#include "src/core/interface/async_context.h"
#include "src/core/interface/service_interface.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/parameter_client/type_def.h"
#include "src/public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Interface responsible for fetching parameters from cloud.
 */
class ParameterClientProviderInterface : public core::ServiceInterface {
 public:
  virtual ~ParameterClientProviderInterface() = default;

  /**
   * @brief Fetches the parameter value.
   *
   * @param context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual core::ExecutionResult GetParameter(
      core::AsyncContext<
          cmrt::sdk::parameter_service::v1::GetParameterRequest,
          cmrt::sdk::parameter_service::v1::GetParameterResponse>&
          context) noexcept = 0;
};

class ParameterClientProviderFactory {
 public:
  /**
   * @brief Factory to create ParameterClientProvider.
   *
   * @param instance_client_provider InstanceClientProvider.
   * @return std::unique_ptr<ParameterClientProviderInterface> created
   * ParameterClientProvider.
   */
  static std::unique_ptr<ParameterClientProviderInterface> Create(
      ParameterClientOptions options,
      InstanceClientProviderInterface* instance_client_provider,
      core::AsyncExecutorInterface* cpu_async_executor,
      core::AsyncExecutorInterface* io_async_executor);
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_INTERFACE_PARAMETER_CLIENT_PROVIDER_INTERFACE_H_
