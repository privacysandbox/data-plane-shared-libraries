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

#include "test_aws_instance_service_factory.h"

#include <memory>
#include <string>

#include "cpio/client_providers/instance_client_provider/test/aws/test_aws_instance_client_provider.h"
#include "cpio/server/src/service_utils.h"

using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::TryReadConfigString;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::TestAwsInstanceClientProvider;
using google::scp::cpio::client_providers::TestInstanceClientOptions;

namespace {
constexpr char kDefaultRegion[] = "us-east-1";
}  // namespace

namespace google::scp::cpio {

ExecutionResult TestAwsInstanceServiceFactory::Init() noexcept {
  region_ = kDefaultRegion;
  TryReadConfigString(
      config_provider_,
      std::dynamic_pointer_cast<TestAwsInstanceServiceFactoryOptions>(options_)
          ->region_config_label,
      region_);

  RETURN_IF_FAILURE(AwsInstanceServiceFactory::Init());

  return SuccessExecutionResult();
}

std::shared_ptr<InstanceClientProviderInterface>
TestAwsInstanceServiceFactory::CreateInstanceClient() noexcept {
  auto options = std::make_shared<TestInstanceClientOptions>();
  options->region = region_;
  return std::make_shared<TestAwsInstanceClientProvider>(options);
}

}  // namespace google::scp::cpio
