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

#pragma once

#include <memory>

#include "core/interface/async_executor_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/parameter_client_provider_interface.h"
#include "cpio/server/src/parameter_service/parameter_service_factory.h"

namespace google::scp::cpio {
/*! @copydoc ParameterServiceFactoryInterface
 */
class GcpParameterServiceFactory : public ParameterServiceFactory {
 public:
  explicit GcpParameterServiceFactory(
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : ParameterServiceFactory(config_provider) {}

  /**
   * @brief Creates ParameterClient.
   *
   * @param config_provider provides configurations
   * @return std::unique_ptr<core::TokenProviderCacheInterface>
   */
  std::shared_ptr<client_providers::ParameterClientProviderInterface>
  CreateParameterClient() noexcept override;

 private:
  std::shared_ptr<InstanceServiceFactoryInterface>
  CreateInstanceServiceFactory() noexcept override;
};
}  // namespace google::scp::cpio
