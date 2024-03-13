/*
 * Copyright 2023 Google LLC
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

#ifndef CPIO_CLIENT_PROVIDERS_BLOB_STORAGE_CLIENT_PROVIDER_GCP_TEST_GCP_BLOB_STORAGE_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_BLOB_STORAGE_CLIENT_PROVIDER_GCP_TEST_GCP_BLOB_STORAGE_CLIENT_PROVIDER_H_

#include <memory>
#include <sstream>
#include <string>

#include "google/cloud/options.h"
#include "src/cpio/client_providers/blob_storage_client_provider/gcp/gcp_blob_storage_client_provider.h"
#include "src/public/cpio/interface/blob_storage_client/type_def.h"

namespace google::scp::cpio::client_providers {
class TestGcpCloudStorageFactory : public GcpCloudStorageFactory {
 public:
  cloud::Options CreateClientOptions(
      BlobStorageClientOptions options) noexcept override;

  virtual ~TestGcpCloudStorageFactory() = default;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_BLOB_STORAGE_CLIENT_PROVIDER_GCP_TEST_GCP_BLOB_STORAGE_CLIENT_PROVIDER_H_
