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

#ifndef CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_GCP_TEE_GCP_KMS_CLIENT_PROVIDER_UTILS_H_
#define CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_GCP_TEE_GCP_KMS_CLIENT_PROVIDER_UTILS_H_

#include <string>

namespace google::scp::cpio::client_providers {
class TeeGcpKmsClientProviderUtils {
 public:
  /**
   * @brief Creates credentials for attestation.
   *
   * @param wip_provider WIP provider.
   * @param service_account_to_impersonate service account to impersonate.
   * @param[out] credential_json credentials in json format.
   */
  static void CreateAttestedCredentials(
      std::string_view wip_provider,
      std::string_view service_account_to_impersonate,
      std::string& credential_json) noexcept;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_GCP_TEE_GCP_KMS_CLIENT_PROVIDER_UTILS_H_
