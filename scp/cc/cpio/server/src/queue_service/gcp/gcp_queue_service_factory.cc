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

#include "gcp_queue_service_factory.h"

#include <memory>
#include <utility>

#include "cpio/client_providers/queue_client_provider/src/gcp/gcp_queue_client_provider.h"
#include "cpio/server/src/instance_service/gcp/gcp_instance_service_factory.h"

using google::scp::cpio::client_providers::GcpQueueClientProvider;
using google::scp::cpio::client_providers::QueueClientOptions;
using google::scp::cpio::client_providers::QueueClientProviderInterface;

namespace google::scp::cpio {
std::shared_ptr<InstanceServiceFactoryInterface>
GcpQueueServiceFactory::CreateInstanceServiceFactory() noexcept {
  return std::make_shared<GcpInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

std::shared_ptr<QueueClientProviderInterface>
GcpQueueServiceFactory::CreateQueueClient() noexcept {
  return std::make_shared<GcpQueueClientProvider>(
      queue_client_options_, instance_client_,
      instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor());
}
}  // namespace google::scp::cpio
