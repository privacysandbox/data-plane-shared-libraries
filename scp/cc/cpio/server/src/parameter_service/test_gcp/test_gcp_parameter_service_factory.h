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

#ifndef CPIO_SERVER_SRC_PARAMETER_SERVICE_TEST_GCP_TEST_GCP_PARAMETER_SERVICE_FACTORY_H_
#define CPIO_SERVER_SRC_PARAMETER_SERVICE_TEST_GCP_TEST_GCP_PARAMETER_SERVICE_FACTORY_H_

#include <memory>

#include "core/interface/config_provider_interface.h"
#include "cpio/server/src/parameter_service/gcp/gcp_parameter_service_factory.h"

namespace google::scp::cpio {
/*! @copydoc ParameterServiceFactoryInterface
 */
class TestGcpParameterServiceFactory : public GcpParameterServiceFactory {
 public:
  explicit TestGcpParameterServiceFactory(
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : GcpParameterServiceFactory(config_provider) {}

 private:
  std::shared_ptr<InstanceServiceFactoryInterface>
  CreateInstanceServiceFactory() noexcept override;

  std::shared_ptr<InstanceServiceFactoryOptions>
  CreateInstanceServiceFactoryOptions() noexcept override;
};
}  // namespace google::scp::cpio

#endif  // CPIO_SERVER_SRC_PARAMETER_SERVICE_TEST_GCP_TEST_GCP_PARAMETER_SERVICE_FACTORY_H_
