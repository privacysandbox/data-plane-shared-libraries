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

#include <nlohmann/json.hpp>

#include "cpio/client_providers/interface/kms_client_provider_interface.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "public/cpio/interface/kms_client/type_def.h"
#include "absl/strings/escaping.h"
#include "absl/log/check.h"

#include "error_codes.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::errors::
    SC_AZURE_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND;
using google::scp::core::errors::SC_AZURE_KMS_CLIENT_PROVIDER_KEY_ID_NOT_FOUND;
using google::scp::core::errors::SC_AZURE_KMS_CLIENT_PROVIDER_BAD_UNWRAPPED_KEY;
using google::scp::core::AsyncContext;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::Uri;
using std::make_shared;
using std::shared_ptr;
using std::all_of;
using std::bind;
using std::cbegin;
using std::cend;
using std::placeholders::_1;
using std::make_pair;
using std::pair;

namespace google::scp::cpio::client_providers {

static constexpr char kAzureKmsClientProvider[] = "AzureKmsClientProvider";

// We need to take this value from a command line option (It already exists somewhere).
constexpr char kKMSUnwrapPath[] =
    "https://127.0.0.1:8000/app/unwrapKey?fmt=tink";

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
  const auto& ciphertext = decrypt_context.request->ciphertext();
  if (ciphertext.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AZURE_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND);
    SCP_ERROR_CONTEXT(kAzureKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to get cipher text from decryption request.");
    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return decrypt_context.result;
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
    return decrypt_context.result;
  }

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();

  http_context.request->path = std::make_shared<Uri>(kKMSUnwrapPath);
  http_context.request->method = HttpMethod::POST;

  // Get Attestation Report
  const auto report = hasSnp() ? fetchSnpAttestation() : fetchFakeSnpAttestation();

  nlohmann::json payload;
  payload["wrapped"] = ciphertext;
  payload["kid"] = key_id;
  payload["attestation"] = report;

  http_context.request->body = core::BytesBuffer(nlohmann::to_string(payload));

  http_context.callback = bind(&AzureKmsClientProvider::OnDecryptCallback,
                               this, decrypt_context, _1);

  auto execution_result = http_client_->PerformRequest(http_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kAzureKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to perform http request to decrypt wrapped key.");

    decrypt_context.result = execution_result;
    decrypt_context.Finish();
    return execution_result;
  }

  return SuccessExecutionResult();
}

void AzureKmsClientProvider::OnDecryptCallback(
    AsyncContext<DecryptRequest, DecryptResponse>&
        decrypt_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context) noexcept {
  if (!http_client_context.result.Successful()) {
    SCP_ERROR_CONTEXT(
        kAzureKmsClientProvider, decrypt_context, http_client_context.result,
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
shared_ptr<KmsClientProviderInterface> KmsClientProviderFactory::Create(
    const shared_ptr<KmsClientOptions>& options,
    const shared_ptr<RoleCredentialsProviderInterface>&
        role_credentials_provider,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) noexcept {
      // We uses GlobalCpio::GetGlobalCpio()->GetHttpClient() to get http_client object instead of
      // adding it to KmsClientProviderFactory::Create() as a new parameter.
      // This is to prevent the existing GCP and AWS implementations from being changed.
      std::shared_ptr<core::HttpClientInterface> http_client;
      auto execution_result =
          GlobalCpio::GetGlobalCpio()->GetHttpClient(http_client);
      CHECK(execution_result.Successful()) << "failed to get http client";
  return make_shared<AzureKmsClientProvider>(http_client);
}
#endif
}  // namespace google::scp::cpio::client_providers