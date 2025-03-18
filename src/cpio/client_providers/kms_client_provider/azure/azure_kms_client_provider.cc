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

#include "azure_kms_client_provider.h"

#include <cstdlib>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <tink/aead.h>

#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "proto/hpke.pb.h"
#include "src/core/utils/base64.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/cpio/client_providers/interface/auth_token_provider_interface.h"
#include "src/cpio/client_providers/interface/kms_client_provider_interface.h"
#include "src/public/cpio/interface/kms_client/type_def.h"

#include "error_codes.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
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

constexpr char kAttestation[] = "attestation";
constexpr char kAzureKmsClientProvider[] = "AzureKmsClientProvider";

constexpr char kDefaultKmsUnwrapPath[] =
    "https://127.0.0.1:8000/app/unwrapKey?fmt=tink";
constexpr char kAzureKmsUnwrapUrlEnvVar[] = "AZURE_BA_PARAM_KMS_UNWRAP_URL";

constexpr char kAuthorizationHeaderKey[] = "Authorization";
constexpr char kBearerTokenPrefix[] = "Bearer ";

// Define properties of API calls
constexpr char kWrappedKid[] = "wrappedKid";
constexpr char kWrapped[] = "wrapped";
constexpr char kWrappingKey[] = "wrappingKey";

absl::Status AzureKmsClientProvider::Decrypt(
    core::AsyncContext<DecryptRequest, DecryptResponse>&
        decrypt_context) noexcept {
  auto get_credentials_request = std::make_shared<GetSessionTokenRequest>();
  AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>
      get_token_context(
          std::move(get_credentials_request),
          absl::bind_front(
              &AzureKmsClientProvider::GetSessionCredentialsCallbackToDecrypt,
              this, decrypt_context),
          decrypt_context);

  if (ExecutionResult execution_result =
          auth_token_provider_->GetSessionToken(get_token_context);
      !execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kAzureKmsClientProvider, decrypt_context,
                      execution_result, "Failed to get the session token.");
    decrypt_context.Finish(execution_result);

    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }

  return absl::OkStatus();
}

void AzureKmsClientProvider::GetSessionCredentialsCallbackToDecrypt(
    core::AsyncContext<DecryptRequest, DecryptResponse>& decrypt_context,
    core::AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context) noexcept {
  if (!get_token_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kAzureKmsClientProvider, decrypt_context,
                      get_token_context.result,
                      "Failed to get the access token.");
    decrypt_context.result = get_token_context.result;
    decrypt_context.Finish();
    return;
  }

  const auto& access_token = *get_token_context.response->session_token;

  const auto& ciphertext = decrypt_context.request->ciphertext();
  if (ciphertext.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AZURE_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND);
    SCP_ERROR_CONTEXT(kAzureKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to get cipher text from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }

  // Check that there is an ID for the key to decrypt with
  const auto& key_id = decrypt_context.request->key_resource_name();
  if (key_id.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_AZURE_KMS_CLIENT_PROVIDER_KEY_ID_NOT_FOUND);
    SCP_ERROR_CONTEXT(kAzureKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to get Key ID from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();

  // For the first call, it tries to get the unwrap URL from environment
  // variable. This is done here because Init() is not called by the shared code
  // and it's a temporary workaround.
  if (unwrap_url_.empty()) {
    const char* value_from_env = std::getenv(kAzureKmsUnwrapUrlEnvVar);
    if (value_from_env) {
      unwrap_url_ = value_from_env;
    } else {
      unwrap_url_ = kDefaultKmsUnwrapPath;
    }
  }
  http_context.request->path = std::make_shared<Uri>(unwrap_url_);
  http_context.request->method = HttpMethod::POST;

  const auto wrapping_key_pair_or = GenerateWrappingKeyPair();
  if (!wrapping_key_pair_or.ok()) {
    std::string error_message = "Failed to generate wrapping key : ";
    error_message += wrapping_key_pair_or.status().ToString().c_str();
    auto execution_result = FailureExecutionResult(
        SC_AZURE_KMS_CLIENT_PROVIDER_WRAPPING_KEY_GENERATION_ERROR);

    SCP_ERROR_CONTEXT(kAzureKmsClientProvider, decrypt_context,
                      execution_result, error_message);
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }
  std::pair<std::shared_ptr<EvpPkeyWrapper>, std::shared_ptr<EvpPkeyWrapper>>
      wrapping_key_pair = wrapping_key_pair_or.value();
  std::string hex_hash_on_wrapping_key = "";
  // Calculate hash on public_key
  const auto hex_hash_on_wrapping_key_or =
      AzureKmsClientProviderUtils::CreateHexHashOnKey(wrapping_key_pair.second);

  if (!hex_hash_on_wrapping_key_or.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_AZURE_KMS_CLIENT_PROVIDER_KEY_HASH_CREATION_ERROR);
    SCP_ERROR_CONTEXT(kAzureKmsClientProvider, decrypt_context,
                      execution_result, "Failed to create hex hash on key: %s.",
                      hex_hash_on_wrapping_key_or.status().ToString().c_str());
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }
  hex_hash_on_wrapping_key = hex_hash_on_wrapping_key_or.value();

  // Get Attestation Report
  const auto report = FetchSnpAttestation(hex_hash_on_wrapping_key);
  CHECK(report.has_value()) << "Failed to get attestation report";

  nlohmann::json payload;
  payload[kWrapped] = ciphertext;
  payload[kWrappedKid] = key_id;
  payload[kAttestation] = nlohmann::json(report.value());
  const auto wrapping_key_or =
      AzureKmsClientProviderUtils::EvpPkeyToPem(wrapping_key_pair.second);
  if (!wrapping_key_or.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_AZURE_KMS_CLIENT_PROVIDER_EVP_TO_PEM_CONVERSION_ERROR);
    SCP_ERROR_CONTEXT(kAzureKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to convert public key to pem: %s.",
                      wrapping_key_or.status().ToString().c_str());
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }
  payload[kWrappingKey] = wrapping_key_or.value();
  http_context.request->body = core::BytesBuffer(nlohmann::to_string(payload));
  http_context.request->headers = std::make_shared<core::HttpHeaders>();
  http_context.request->headers->insert(
      {std::string(kAuthorizationHeaderKey),
       absl::StrCat(kBearerTokenPrefix, access_token)});

  http_context.callback = bind(&AzureKmsClientProvider::OnDecryptCallback, this,
                               decrypt_context, wrapping_key_pair.first, _1);

  auto execution_result = http_client_->PerformRequest(http_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kAzureKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to perform http request to decrypt wrapped key.");

    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }
}

void AzureKmsClientProvider::OnDecryptCallback(
    AsyncContext<DecryptRequest, DecryptResponse>& decrypt_context,
    std::shared_ptr<EvpPkeyWrapper> ephemeral_private_key,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context) noexcept {
  if (!http_client_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kAzureKmsClientProvider, decrypt_context,
                      http_client_context.result,
                      "Failed to decrypt wrapped key using Azure KMS");
    decrypt_context.result = http_client_context.result;
    decrypt_context.Finish();
    return;
  }

  std::string resp(http_client_context.response->body.bytes->begin(),
                   http_client_context.response->body.bytes->end());
  nlohmann::json unwrapResp;
  try {
    unwrapResp = nlohmann::json::parse(resp);
  } catch (const nlohmann::json::parse_error& e) {
    SCP_ERROR_CONTEXT(kAzureKmsClientProvider, decrypt_context,
                      http_client_context.result,
                      "Failed to parse response from Azure KMS unwrapKey");
    decrypt_context.result = http_client_context.result;
    decrypt_context.Finish();
    return;
  }
  std::string base64_encoded_str = unwrapResp[kWrapped].get<std::string>();
  std::string decodedWrapped;
  if (auto execution_result =
          Base64Decode(std::string_view(base64_encoded_str), decodedWrapped);
      !execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kAzureKmsClientProvider, decrypt_context, http_client_context.result,
        "Failed to base64 decode response from Azure KMS unwrapKey");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }
  std::vector<uint8_t> encrypted(decodedWrapped.begin(), decodedWrapped.end());

  const auto decrypted_or =
      AzureKmsClientProviderUtils::KeyUnwrap(ephemeral_private_key, encrypted);
  if (!decrypted_or.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_AZURE_KMS_CLIENT_PROVIDER_UNWRAPPING_DECRYPTED_KEY_ERROR);
    SCP_ERROR_CONTEXT(kAzureKmsClientProvider, decrypt_context,
                      execution_result, "Failed to unwrap decrypted key: %s.",
                      decrypted_or.status().ToString().c_str());
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return;
  }
  const auto decrypted = decrypted_or.value();
  decrypt_context.response = std::make_shared<DecryptResponse>();

  decrypt_context.response->set_plaintext(decrypted);

  decrypt_context.result = SuccessExecutionResult();
  decrypt_context.Finish();
}

std::unique_ptr<KmsClientProviderInterface> KmsClientProviderFactory::Create(
    absl::Nonnull<
        RoleCredentialsProviderInterface*> /*role_credentials_provider*/,
    AsyncExecutorInterface* /*io_async_executor*/) noexcept {
  // We uses GlobalCpio::GetGlobalCpio()->GetHttpClient() to get http_client
  // object instead of adding it to KmsClientProviderFactory::Create() as a new
  // parameter. This is to prevent the existing GCP and AWS implementations from
  // being changed.
  auto cpio_ = &GlobalCpio::GetGlobalCpio();
  auto http_client = &cpio_->GetHttpClient();

  auto auth_token_provider = &cpio_->GetAuthTokenProvider();

  return std::make_unique<AzureKmsClientProvider>(http_client,
                                                  auth_token_provider);
}
}  // namespace google::scp::cpio::client_providers
