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

#include "cpio/server/src/blob_storage_service/aws/aws_blob_storage_service_factory.h"
#include "public/cpio/test/blob_storage_client/test_aws_blob_storage_client_options.h"

namespace google::scp::cpio {
/*! @copydoc BlobStorageServiceFactoryInterface
 */
class TestAwsBlobStorageServiceFactory : public AwsBlobStorageServiceFactory {
 public:
  explicit TestAwsBlobStorageServiceFactory(
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : AwsBlobStorageServiceFactory(config_provider),
        test_options_(std::make_shared<TestAwsBlobStorageClientOptions>()) {}

  std::shared_ptr<client_providers::BlobStorageClientProviderInterface>
  CreateBlobStorageClient() noexcept override;

 protected:
  std::shared_ptr<InstanceServiceFactoryOptions>
  CreateInstanceServiceFactoryOptions() noexcept override;

 private:
  std::shared_ptr<InstanceServiceFactoryInterface>
  CreateInstanceServiceFactory() noexcept override;

  std::shared_ptr<TestAwsBlobStorageClientOptions> test_options_;
};

}  // namespace google::scp::cpio
