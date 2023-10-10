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

#ifndef CPIO_SERVER_SRC_BLOB_STORAGE_SERVICE_GCP_GCP_BLOB_STORAGE_SERVICE_FACTORY_H_
#define CPIO_SERVER_SRC_BLOB_STORAGE_SERVICE_GCP_GCP_BLOB_STORAGE_SERVICE_FACTORY_H_

#include <memory>

#include "cpio/client_providers/interface/blob_storage_client_provider_interface.h"
#include "cpio/server/src/blob_storage_service/blob_storage_service_factory.h"

namespace google::scp::cpio {
/*! @copydoc BlobStorageServiceFactoryInterface
 */
class GcpBlobStorageServiceFactory : public BlobStorageServiceFactory {
 public:
  using BlobStorageServiceFactory::BlobStorageServiceFactory;

  /**
   * @brief Creates NoSQLClient.
   *
   * @return
   * std::shared_ptr<client_providers::BlobStorageClientProviderInterface>
   * created parameter client.
   */
  std::shared_ptr<client_providers::BlobStorageClientProviderInterface>
  CreateBlobStorageClient() noexcept override;

 private:
  std::shared_ptr<InstanceServiceFactoryInterface>
  CreateInstanceServiceFactory() noexcept override;
};

}  // namespace google::scp::cpio

#endif  // CPIO_SERVER_SRC_BLOB_STORAGE_SERVICE_GCP_GCP_BLOB_STORAGE_SERVICE_FACTORY_H_
