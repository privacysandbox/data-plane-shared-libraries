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

#include <memory>

#include "google/cloud/kms/key_management_client.h"
#include "src/cpio/client_providers/kms_client_provider/gcp/gcp_kms_client_provider.h"

using google::cloud::kms::KeyManagementServiceClient;
using google::cloud::kms::MakeKeyManagementServiceConnection;

namespace google::scp::cpio::client_providers {

std::shared_ptr<KeyManagementServiceClient>
GcpKmsAeadProvider::CreateKeyManagementServiceClient(
    std::string_view wip_provider,
    std::string_view service_account_to_impersonate) noexcept {
  return std::make_shared<KeyManagementServiceClient>(
      MakeKeyManagementServiceConnection());
}
}  // namespace google::scp::cpio::client_providers
