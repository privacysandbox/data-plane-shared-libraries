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

#include "gcp_blob_storage_service_factory.h"

#include <memory>
#include <utility>

#include "cpio/client_providers/blob_storage_client_provider/src/gcp/gcp_blob_storage_client_provider.h"
#include "cpio/client_providers/interface/blob_storage_client_provider_interface.h"
#include "cpio/server/interface/blob_storage_service/blob_storage_service_factory_interface.h"
#include "cpio/server/interface/blob_storage_service/configuration_keys.h"
#include "cpio/server/src/instance_service/gcp/gcp_instance_service_factory.h"

using google::scp::core::ExecutionResult;
using google::scp::cpio::client_providers::BlobStorageClientProviderInterface;
using google::scp::cpio::client_providers::GcpBlobStorageClientProvider;
using std::make_shared;
using std::shared_ptr;

namespace google::scp::cpio {

shared_ptr<InstanceServiceFactoryInterface>
GcpBlobStorageServiceFactory::CreateInstanceServiceFactory() noexcept {
  return make_shared<GcpInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

std::shared_ptr<BlobStorageClientProviderInterface>
GcpBlobStorageServiceFactory::CreateBlobStorageClient() noexcept {
  return make_shared<GcpBlobStorageClientProvider>(
      make_shared<BlobStorageClientOptions>(), instance_client_,
      instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor());
}

}  // namespace google::scp::cpio
