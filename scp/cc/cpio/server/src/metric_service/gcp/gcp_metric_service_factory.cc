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

#include "gcp_metric_service_factory.h"

#include <memory>
#include <utility>

#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "cpio/client_providers/metric_client_provider/src/gcp/gcp_metric_client_provider.h"
#include "cpio/server/interface/metric_service/metric_service_factory_interface.h"
#include "cpio/server/src/instance_service/gcp/gcp_instance_service_factory.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"

using google::scp::cpio::MetricClientInterface;
using google::scp::cpio::client_providers::GcpMetricClientProvider;
using std::make_shared;
using std::shared_ptr;

namespace google::scp::cpio {
shared_ptr<InstanceServiceFactoryInterface>
GcpMetricServiceFactory::CreateInstanceServiceFactory() noexcept {
  return make_shared<GcpInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

std::shared_ptr<MetricClientInterface>
GcpMetricServiceFactory::CreateMetricClient() noexcept {
  return make_shared<GcpMetricClientProvider>(nullptr /*MetricClientOptions>*/,
                                              instance_client_,
                                              nullptr /*AsyncExecutor*/);
}
}  // namespace google::scp::cpio
