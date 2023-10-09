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

#include "test_aws_parameter_service_factory.h"

#include <memory>

#include "cpio/client_providers/parameter_client_provider/src/aws/aws_parameter_client_provider.h"
#include "cpio/client_providers/parameter_client_provider/test/aws/test_aws_parameter_client_provider.h"
#include "cpio/server/src/instance_service/test_aws/test_aws_instance_service_factory.h"
#include "cpio/server/src/service_utils.h"

#include "test_configuration_keys.h"

using google::scp::cpio::client_providers::AwsParameterClientProvider;
using google::scp::cpio::client_providers::ParameterClientProviderInterface;
using google::scp::cpio::client_providers::TestAwsParameterClientProvider;
using std::make_shared;
using std::shared_ptr;

namespace google::scp::cpio {
shared_ptr<InstanceServiceFactoryInterface>
TestAwsParameterServiceFactory::CreateInstanceServiceFactory() noexcept {
  return make_shared<TestAwsInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

shared_ptr<InstanceServiceFactoryOptions>
TestAwsParameterServiceFactory::CreateInstanceServiceFactoryOptions() noexcept {
  auto options = make_shared<TestAwsInstanceServiceFactoryOptions>();
  options->region_config_label = kTestAwsParameterClientRegion;
  return options;
}

std::shared_ptr<ParameterClientProviderInterface>
TestAwsParameterServiceFactory::CreateParameterClient() noexcept {
  auto execution_result = TryReadConfigString(
      config_provider_, kTestParameterClientCloudEndpointOverride,
      *test_options_->ssm_endpoint_override);
  if (execution_result.Successful() &&
      !test_options_->ssm_endpoint_override->empty()) {
    return make_shared<TestAwsParameterClientProvider>(
        test_options_, instance_client_,
        instance_service_factory_->GetIoAsynceExecutor());
  }
  return make_shared<AwsParameterClientProvider>(
      test_options_, instance_client_,
      instance_service_factory_->GetIoAsynceExecutor());
}
}  // namespace google::scp::cpio
