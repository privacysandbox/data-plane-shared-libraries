
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

#include "test_gcp_blob_storage_client_provider.h"

#include <utility>

#include "google/cloud/credentials.h"
#include "google/cloud/options.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/blob_storage_client/type_def.h"
#include "src/public/cpio/test/blob_storage_client/test_gcp_blob_storage_client_options.h"

using google::cloud::MakeGoogleDefaultCredentials;
using google::cloud::MakeImpersonateServiceAccountCredentials;
using google::cloud::Options;
using google::scp::core::AsyncExecutorInterface;

namespace google::scp::cpio::client_providers {
Options TestGcpCloudStorageFactory::CreateClientOptions(
    BlobStorageClientOptions options) noexcept {
  Options client_options = GcpCloudStorageFactory::CreateClientOptions(options);
  auto test_options =
      std::move(dynamic_cast<TestGcpBlobStorageClientOptions&>(options));
  if (!test_options.impersonate_service_account.empty()) {
    client_options.set<google::cloud::UnifiedCredentialsOption>(
        (MakeImpersonateServiceAccountCredentials(
            google::cloud::MakeGoogleDefaultCredentials(),
            std::move(test_options.impersonate_service_account))));
  }
  return client_options;
}
}  // namespace google::scp::cpio::client_providers
