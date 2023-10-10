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

#ifndef CPIO_SERVER_SRC_AUTO_SCALING_SERVICE_AWS_AWS_AUTO_SCALING_SERVICE_FACTORY_H_
#define CPIO_SERVER_SRC_AUTO_SCALING_SERVICE_AWS_AWS_AUTO_SCALING_SERVICE_FACTORY_H_

#include <memory>

#include "core/interface/config_provider_interface.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cpio/client_providers/interface/auto_scaling_client_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/server/interface/auto_scaling_service/auto_scaling_service_factory_interface.h"
#include "cpio/server/interface/instance_service/instance_service_factory_interface.h"

namespace google::scp::cpio {
/*! @copydoc AutoScalingServiceFactoryInterface
 */
class AwsAutoScalingServiceFactory : public AutoScalingServiceFactoryInterface {
 public:
  explicit AwsAutoScalingServiceFactory(
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : config_provider_(config_provider) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  /**
   * @brief Creates AutoScalingClient.
   *
   * @return
   * std::shared_ptr<client_providers::AutoScalingClientProviderInterface>
   * created auto_scaling client.
   */
  std::shared_ptr<client_providers::AutoScalingClientProviderInterface>
  CreateAutoScalingClient() noexcept override;

 protected:
  std::shared_ptr<core::ConfigProviderInterface> config_provider_;
  std::shared_ptr<InstanceServiceFactoryInterface> instance_service_factory_;
  std::shared_ptr<InstanceServiceFactoryOptions>
      instance_service_factory_options_;
  std::shared_ptr<client_providers::InstanceClientProviderInterface>
      instance_client_;

 private:
  virtual std::shared_ptr<InstanceServiceFactoryInterface>
  CreateInstanceServiceFactory() noexcept;

  virtual std::shared_ptr<InstanceServiceFactoryOptions>
  CreateInstanceServiceFactoryOptions() noexcept;
};
}  // namespace google::scp::cpio

#endif  // CPIO_SERVER_SRC_AUTO_SCALING_SERVICE_AWS_AWS_AUTO_SCALING_SERVICE_FACTORY_H_
