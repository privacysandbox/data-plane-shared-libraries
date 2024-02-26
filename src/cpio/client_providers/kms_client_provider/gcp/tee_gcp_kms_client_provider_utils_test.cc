// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/cpio/client_providers/kms_client_provider/gcp/tee_gcp_kms_client_provider_utils.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace google::scp::cpio::client_providers::test {
namespace {
constexpr std::string_view kWipProvider = "wip";
constexpr std::string_view kServiceAccount = "service_account";

std::string removeWhitespace(std::string&& str) {
  str.erase(std::remove_if(str.begin(), str.end(),
                           [](unsigned char x) { return std::isspace(x); }),
            str.end());

  return std::move(str);
}

TEST(GcpKmsClientProviderUtilsTest, CreateAttestedCredentials) {
  std::string credentials;
  TeeGcpKmsClientProviderUtils::CreateAttestedCredentials(
      kWipProvider, kServiceAccount, credentials);

  std::string expectedCreds = removeWhitespace(R"({
      "audience": "//iam.googleapis.com/wip",
      "credential_source": {
        "file": "/run/container_launcher/attestation_verifier_claims_token"
      },
      "service_account_impersonation_url": "https://iamcredentials.googleapis.com
        /v1/projects/-/serviceAccounts/service_account:generateAccessToken",
      "subject_token_type": "urn:ietf:params:oauth:token-type:jwt",
      "token_url": "https://sts.googleapis.com/v1/token",
      "type": "external_account"
  })");

  EXPECT_EQ(credentials, expectedCreds);
}
}  // namespace
}  // namespace google::scp::cpio::client_providers::test
