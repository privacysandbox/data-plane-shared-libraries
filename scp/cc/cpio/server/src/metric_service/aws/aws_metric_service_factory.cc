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

#include "aws_metric_service_factory.h"

#include <memory>

#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "cpio/client_providers/metric_client_provider/src/aws/aws_metric_client_provider.h"
#include "cpio/server/interface/metric_service/metric_service_factory_interface.h"
#include "cpio/server/src/instance_service/aws/aws_instance_service_factory.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/interface/metric_client/type_def.h"

using google::scp::cpio::MetricClientInterface;
using google::scp::cpio::client_providers::AwsMetricClientProvider;

namespace google::scp::cpio {
std::shared_ptr<InstanceServiceFactoryInterface>
AwsMetricServiceFactory::CreateInstanceServiceFactory() noexcept {
  return std::make_shared<AwsInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

std::shared_ptr<MetricClientInterface>
AwsMetricServiceFactory::CreateMetricClient() noexcept {
  return std::make_shared<AwsMetricClientProvider>(
      std::make_shared<MetricClientOptions>(), instance_client_,
      instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor());
}
}  // namespace google::scp::cpio
