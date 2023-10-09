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

#include "gcp_parameter_service_factory.h"

#include <memory>
#include <utility>

#include "cpio/client_providers/interface/parameter_client_provider_interface.h"
#include "cpio/client_providers/parameter_client_provider/src/gcp/gcp_parameter_client_provider.h"
#include "cpio/server/interface/parameter_service/parameter_service_factory_interface.h"
#include "cpio/server/src/instance_service/gcp/gcp_instance_service_factory.h"
#include "public/cpio/interface/parameter_client/type_def.h"

using google::scp::cpio::client_providers::GcpParameterClientProvider;
using google::scp::cpio::client_providers::ParameterClientProviderInterface;
using std::make_shared;
using std::shared_ptr;

namespace google::scp::cpio {
shared_ptr<InstanceServiceFactoryInterface>
GcpParameterServiceFactory::CreateInstanceServiceFactory() noexcept {
  return make_shared<GcpInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

std::shared_ptr<ParameterClientProviderInterface>
GcpParameterServiceFactory::CreateParameterClient() noexcept {
  return make_shared<GcpParameterClientProvider>(
      instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor(), instance_client_,
      make_shared<ParameterClientOptions>());
}
}  // namespace google::scp::cpio
