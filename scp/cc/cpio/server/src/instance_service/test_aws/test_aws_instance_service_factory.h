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
#include <string>

#include "core/interface/config_provider_interface.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/server/interface/instance_service/instance_service_factory_interface.h"
#include "cpio/server/src/instance_service/aws/aws_instance_service_factory.h"

namespace google::scp::cpio {
struct TestAwsInstanceServiceFactoryOptions
    : public InstanceServiceFactoryOptions {
  std::string region_config_label;
};

/*! @copydoc InstanceServiceFactoryInterface
 */
class TestAwsInstanceServiceFactory : public AwsInstanceServiceFactory {
 public:
  using AwsInstanceServiceFactory::AwsInstanceServiceFactory;

  core::ExecutionResult Init() noexcept override;

 private:
  std::shared_ptr<client_providers::InstanceClientProviderInterface>
  CreateInstanceClient() noexcept override;

  std::string region_;
};

}  // namespace google::scp::cpio
