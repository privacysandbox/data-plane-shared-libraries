/*
 * Portions Copyright (c) Microsoft Corporation
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

#include <cstdlib>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <tink/aead.h>

#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "proto/hpke.pb.h"
#include "src/azure/attestation/src/attestation.h"
#include "src/core/utils/base64.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/cpio/client_providers/interface/auth_token_provider_interface.h"
#include "src/cpio/client_providers/interface/kms_client_provider_interface.h"
#include "src/public/cpio/interface/kms_client/type_def.h"

#include "azure_kms_client_provider.h"
#include "error_codes.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::azure::attestation::fetchFakeSnpAttestation;
using google::scp::azure::attestation::fetchSnpAttestation;
using google::scp::azure::attestation::hasSnp;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::SC_AZURE_KMS_CLIENT_PROVIDER_BAD_UNWRAPPED_KEY;
using google::scp::core::errors::
    SC_AZURE_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND;
using google::scp::core::errors::
    SC_AZURE_KMS_CLIENT_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND;
using google::scp::core::errors::
    SC_AZURE_KMS_CLIENT_PROVIDER_EVP_TO_PEM_CONVERSION_ERROR;
using google::scp::core::errors::
    SC_AZURE_KMS_CLIENT_PROVIDER_KEY_HASH_CREATION_ERROR;
using google::scp::core::errors::SC_AZURE_KMS_CLIENT_PROVIDER_KEY_ID_NOT_FOUND;
using google::scp::core::errors::
    SC_AZURE_KMS_CLIENT_PROVIDER_UNWRAPPING_DECRYPTED_KEY_ERROR;
using google::scp::core::errors::
    SC_AZURE_KMS_CLIENT_PROVIDER_WRAPPING_KEY_GENERATION_ERROR;
using google::scp::core::utils::Base64Decode;
using google::scp::core::utils::Base64Encode;
using google::scp::cpio::client_providers::AzureKmsClientProviderUtils;
using google::scp::cpio::client_providers::EvpPkeyWrapper;
using std::all_of;
using std::bind;
using std::cbegin;
using std::cend;
using std::make_pair;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::placeholders::_1;

namespace google::scp::cpio::client_providers {

absl::StatusOr<
    std::pair<std::shared_ptr<EvpPkeyWrapper>, std::shared_ptr<EvpPkeyWrapper>>>
AzureKmsClientProvider::GenerateWrappingKeyPair() noexcept {
  CHECK(hasSnp()) << "It's not in a SNP environment";
  return AzureKmsClientProviderUtils::GenerateWrappingKey();
}

std::optional<azure::attestation::AttestationReport>
AzureKmsClientProvider::FetchSnpAttestation(
    const std::string report_data) noexcept {
  CHECK(hasSnp()) << "It's not in a SNP environment";
  return fetchSnpAttestation(report_data);
}

}  // namespace google::scp::cpio::client_providers
