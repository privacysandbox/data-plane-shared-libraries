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

#include "cpio/server/src/blob_storage_service/test_aws/test_aws_blob_storage_service_factory.h"

#include <memory>

#include "cpio/client_providers/blob_storage_client_provider/src/aws/aws_blob_storage_client_provider.h"
#include "cpio/client_providers/blob_storage_client_provider/test/aws/test_aws_blob_storage_client_provider.h"
#include "cpio/server/src/instance_service/test_aws/test_aws_instance_service_factory.h"
#include "cpio/server/src/service_utils.h"

#include "test_configuration_keys.h"

using google::scp::cpio::client_providers::AwsBlobStorageClientProvider;
using google::scp::cpio::client_providers::BlobStorageClientProviderInterface;
using google::scp::cpio::client_providers::TestAwsBlobStorageClientProvider;

namespace google::scp::cpio {
std::shared_ptr<InstanceServiceFactoryInterface>
TestAwsBlobStorageServiceFactory::CreateInstanceServiceFactory() noexcept {
  return std::make_shared<TestAwsInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

std::shared_ptr<InstanceServiceFactoryOptions>
TestAwsBlobStorageServiceFactory::
    CreateInstanceServiceFactoryOptions() noexcept {
  auto options = std::make_shared<TestAwsInstanceServiceFactoryOptions>();
  options->region_config_label = kTestAwsBlobStorageClientRegion;
  return options;
}

std::shared_ptr<BlobStorageClientProviderInterface>
TestAwsBlobStorageServiceFactory::CreateBlobStorageClient() noexcept {
  auto execution_result = TryReadConfigString(
      config_provider_, kTestBlobStorageClientCloudEndpointOverride,
      *test_options_->s3_endpoint_override);
  if (execution_result.Successful() &&
      !test_options_->s3_endpoint_override->empty()) {
    return std::make_shared<TestAwsBlobStorageClientProvider>(
        test_options_, instance_client_,
        instance_service_factory_->GetCpuAsynceExecutor(),
        instance_service_factory_->GetIoAsynceExecutor());
  }
  return std::make_shared<AwsBlobStorageClientProvider>(
      test_options_, instance_client_,
      instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor());
}
}  // namespace google::scp::cpio
