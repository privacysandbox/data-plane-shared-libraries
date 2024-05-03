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

#include <nlohmann/json.hpp>

#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/cpio/client_providers/interface/auth_token_provider_interface.h"
#include "src/cpio/client_providers/interface/kms_client_provider_interface.h"
#include "src/public/cpio/interface/kms_client/type_def.h"

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
using google::scp::core::errors::SC_AZURE_KMS_CLIENT_PROVIDER_BAD_UNWRAPPED_KEY;
using google::scp::core::errors::
    SC_AZURE_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND;
using google::scp::core::errors::SC_AZURE_KMS_CLIENT_PROVIDER_KEY_ID_NOT_FOUND;
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

static constexpr char kAzureKmsClientProvider[] = "AzureKmsClientProvider";

constexpr char kDefaultKmsUnwrapPath[] =
    "https://127.0.0.1:8000/app/unwrapKey?fmt=tink";
constexpr char kAzureKmsUnwrapUrlEnvVar[] = "AZURE_BA_PARAM_KMS_UNWRAP_URL";

constexpr char kAuthorizationHeaderKey[] = "Authorization";
constexpr char kBearerTokenPrefix[] = "Bearer ";

ExecutionResult AzureKmsClientProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AzureKmsClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AzureKmsClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AzureKmsClientProvider::Decrypt(
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

  return auth_token_provider_->GetSessionToken(get_token_context);
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

  // Get Attestation Report
  const auto report =
      hasSnp() ? fetchSnpAttestation() : fetchFakeSnpAttestation();
  CHECK(report.has_value()) << "Failed to get attestation report";

  nlohmann::json payload;
  payload["wrapped"] = ciphertext;
  payload["kid"] = key_id;
  payload["attestation"] = nlohmann::json(report.value());

  http_context.request->body = core::BytesBuffer(nlohmann::to_string(payload));
  http_context.request->headers = std::make_shared<core::HttpHeaders>();
  http_context.request->headers->insert(
      {std::string(kAuthorizationHeaderKey),
       absl::StrCat(kBearerTokenPrefix, access_token)});

  http_context.callback = bind(&AzureKmsClientProvider::OnDecryptCallback, this,
                               decrypt_context, _1);

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

  decrypt_context.response = std::make_shared<DecryptResponse>();

  decrypt_context.response->set_plaintext(resp);

  decrypt_context.result = SuccessExecutionResult();
  decrypt_context.Finish();
}

#ifndef TEST_CPIO
std::unique_ptr<KmsClientProviderInterface> KmsClientProviderFactory::Create(
    KmsClientOptions options,
    RoleCredentialsProviderInterface* role_credentials_provider,
    AsyncExecutorInterface* io_async_executor) noexcept {
  // We uses GlobalCpio::GetGlobalCpio()->GetHttpClient() to get http_client
  // object instead of adding it to KmsClientProviderFactory::Create() as a new
  // parameter. This is to prevent the existing GCP and AWS implementations from
  // being changed.
  auto cpio_ = &GlobalCpio::GetGlobalCpio();
  HttpClientInterface* http_client;
  auto client = cpio_->GetHttpClient();
  CHECK(client.ok()) << "failed to get http client";
  http_client = *client;

  AuthTokenProviderInterface* auth_token_provider;
  auto provider = cpio_->GetAuthTokenProvider();
  CHECK(provider.ok()) << "failed to get auth tokeb provider";
  auth_token_provider = *provider;

  return std::make_unique<AzureKmsClientProvider>(http_client,
                                                  auth_token_provider);
}
#endif
}  // namespace google::scp::cpio::client_providers
