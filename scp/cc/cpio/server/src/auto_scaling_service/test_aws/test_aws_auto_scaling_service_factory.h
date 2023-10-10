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

#ifndef CPIO_SERVER_SRC_AUTO_SCALING_SERVICE_TEST_AWS_TEST_AWS_AUTO_SCALING_SERVICE_FACTORY_H_
#define CPIO_SERVER_SRC_AUTO_SCALING_SERVICE_TEST_AWS_TEST_AWS_AUTO_SCALING_SERVICE_FACTORY_H_

#include <memory>

#include "core/interface/config_provider_interface.h"
#include "cpio/client_providers/auto_scaling_client_provider/test/aws/test_aws_auto_scaling_client_provider.h"
#include "cpio/server/src/auto_scaling_service/aws/aws_auto_scaling_service_factory.h"

namespace google::scp::cpio {
/*! @copydoc AutoScalingServiceFactoryInterface
 */
class TestAwsAutoScalingServiceFactory : public AwsAutoScalingServiceFactory {
 public:
  explicit TestAwsAutoScalingServiceFactory(
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : AwsAutoScalingServiceFactory(config_provider),
        test_options_(std::make_shared<
                      client_providers::TestAwsAutoScalingClientOptions>()) {}

  std::shared_ptr<client_providers::AutoScalingClientProviderInterface>
  CreateAutoScalingClient() noexcept override;

 private:
  std::shared_ptr<InstanceServiceFactoryInterface>
  CreateInstanceServiceFactory() noexcept override;

  std::shared_ptr<InstanceServiceFactoryOptions>
  CreateInstanceServiceFactoryOptions() noexcept override;

  std::shared_ptr<client_providers::TestAwsAutoScalingClientOptions>
      test_options_;
};
}  // namespace google::scp::cpio

#endif  // CPIO_SERVER_SRC_AUTO_SCALING_SERVICE_TEST_AWS_TEST_AWS_AUTO_SCALING_SERVICE_FACTORY_H_
