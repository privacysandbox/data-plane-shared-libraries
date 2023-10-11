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

#include "test_aws_queue_service_factory.h"

#include <memory>

#include "cpio/server/src/instance_service/test_aws/test_aws_instance_service_factory.h"

#include "test_configuration_keys.h"

namespace google::scp::cpio {
std::shared_ptr<InstanceServiceFactoryInterface>
TestAwsQueueServiceFactory::CreateInstanceServiceFactory() noexcept {
  return std::make_shared<TestAwsInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

std::shared_ptr<InstanceServiceFactoryOptions>
TestAwsQueueServiceFactory::CreateInstanceServiceFactoryOptions() noexcept {
  auto options = std::make_shared<TestAwsInstanceServiceFactoryOptions>();
  options->region_config_label = kTestAwsQueueClientRegion;
  return options;
}
}  // namespace google::scp::cpio
