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

#include "src/cpio/client_providers/kms_client_provider/gcp/tee_gcp_kms_client_provider_utils.h"

#include <string_view>

#include <nlohmann/json.hpp>

#include "absl/strings/substitute.h"

namespace {

constexpr std::string_view kAudience = "//iam.googleapis.com/$0";
constexpr std::string_view kImpersonationUrl =
    "https://iamcredentials.googleapis.com/v1/projects/-/serviceAccounts/"
    "$0:generateAccessToken";
}  // namespace

namespace google::scp::cpio::client_providers {
void TeeGcpKmsClientProviderUtils::CreateAttestedCredentials(
    std::string_view wip_provider,
    std::string_view service_account_to_impersonate,
    std::string& credential_json) noexcept {
  const nlohmann::json configuration{
      {"type", "external_account"},
      {"audience", absl::Substitute(kAudience, wip_provider)},
      {"subject_token_type", "urn:ietf:params:oauth:token-type:jwt"},
      {"token_url", "https://sts.googleapis.com/v1/token"},
      {"service_account_impersonation_url",
       absl::Substitute(kImpersonationUrl, service_account_to_impersonate)},
      {"credential_source",
       nlohmann::json{
           {"file",
            "/run/container_launcher/attestation_verifier_claims_token"}}},
  };

  credential_json = configuration.dump();
}
}  // namespace google::scp::cpio::client_providers
