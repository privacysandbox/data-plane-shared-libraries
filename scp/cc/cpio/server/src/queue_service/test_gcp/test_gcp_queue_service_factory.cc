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

#include "test_gcp_queue_service_factory.h"

#include <memory>

#include "cpio/client_providers/queue_client_provider/test/gcp/test_gcp_queue_client_provider.h"
#include "cpio/server/interface/queue_service/configuration_keys.h"
#include "cpio/server/src/instance_service/test_gcp/test_gcp_instance_service_factory.h"
#include "cpio/server/src/service_utils.h"

#include "test_configuration_keys.h"

using google::scp::cpio::client_providers::QueueClientOptions;
using google::scp::cpio::client_providers::QueueClientProviderInterface;
using google::scp::cpio::client_providers::TestGcpQueueClientOptions;
using google::scp::cpio::client_providers::TestGcpQueueClientProvider;
using std::dynamic_pointer_cast;

namespace google::scp::cpio {
std::shared_ptr<QueueClientOptions>
TestGcpQueueServiceFactory::CreateQueueClientOptions() noexcept {
  auto test_options = std::make_shared<TestGcpQueueClientOptions>();
  test_options->queue_name =
      ReadConfigString(config_provider_, kQueueClientQueueName);

  TryReadConfigString(config_provider_,
                      kTestGcpQueueClientCloudEndpointOverride,
                      *test_options->pubsub_client_endpoint_override);

  return test_options;
}

std::shared_ptr<QueueClientProviderInterface>
TestGcpQueueServiceFactory::CreateQueueClient() noexcept {
  return std::make_shared<TestGcpQueueClientProvider>(
      dynamic_pointer_cast<TestGcpQueueClientOptions>(queue_client_options_),
      instance_client_, instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor());
}

std::shared_ptr<InstanceServiceFactoryInterface>
TestGcpQueueServiceFactory::CreateInstanceServiceFactory() noexcept {
  return std::make_shared<TestGcpInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

std::shared_ptr<InstanceServiceFactoryOptions>
TestGcpQueueServiceFactory::CreateInstanceServiceFactoryOptions() noexcept {
  auto options = std::make_shared<TestGcpInstanceServiceFactoryOptions>();
  options->project_id_config_label = kTestGcpQueueClientProjectId;
  return options;
}
}  // namespace google::scp::cpio
