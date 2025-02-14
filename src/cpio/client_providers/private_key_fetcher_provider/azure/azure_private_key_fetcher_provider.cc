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

#include "azure_private_key_fetcher_provider.h"

#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/functional/bind_front.h"
#include "absl/strings/str_cat.h"
#include "src/azure/attestation/src/attestation.h"
#include "src/core/interface/http_client_interface.h"
#include "src/cpio/client_providers/interface/auth_token_provider_interface.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "src/cpio/client_providers/private_key_fetcher_provider/private_key_fetcher_provider_utils.h"

#include "azure_private_key_fetcher_provider_utils.h"
#include "error_codes.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_AZURE_PRIVATE_KEY_FETCHER_CREDENTIALS_PROVIDER_NOT_FOUND;
using std::bind;
using std::placeholders::_1;

namespace {
constexpr char kAzurePrivateKeyFetcherProvider[] =
    "AzurePrivateKeyFetcherProvider";
constexpr char kAuthorizationHeaderKey[] = "Authorization";
constexpr char kBearerTokenPrefix[] = "Bearer ";

// Define properties of API calls
constexpr char kWrappedKid[] = "wrappedKid";
constexpr char kWrapped[] = "wrapped";

}  // namespace

namespace google::scp::cpio::client_providers {

ExecutionResult AzurePrivateKeyFetcherProvider::Init() noexcept {
  RETURN_IF_FAILURE(PrivateKeyFetcherProvider::Init());

  if (!auth_token_provider_) {
    auto execution_result = FailureExecutionResult(
        SC_AZURE_PRIVATE_KEY_FETCHER_CREDENTIALS_PROVIDER_NOT_FOUND);
    SCP_ERROR(kAzurePrivateKeyFetcherProvider, kZeroUuid, execution_result,
              "Failed to get credentials provider.");
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult AzurePrivateKeyFetcherProvider::SignHttpRequest(
    AsyncContext<PrivateKeyFetchingRequest, core::HttpRequest>&
        sign_request_context) noexcept {
  auto request = std::make_shared<GetSessionTokenRequest>();
  AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>
      get_token_context(
          std::move(request),
          absl::bind_front(
              &AzurePrivateKeyFetcherProvider::OnGetSessionTokenCallback, this,
              sign_request_context),
          sign_request_context);

  return auth_token_provider_->GetSessionToken(get_token_context);
}

void AzurePrivateKeyFetcherProvider::OnGetSessionTokenCallback(
    AsyncContext<PrivateKeyFetchingRequest, core::HttpRequest>&
        sign_request_context,
    AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context) noexcept {
  if (!get_token_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kAzurePrivateKeyFetcherProvider, sign_request_context,
                      get_token_context.result,
                      "Failed to get the access token.");
    sign_request_context.result = get_token_context.result;
    sign_request_context.Finish();
    return;
  }

  const auto& access_token = *get_token_context.response->session_token;
  auto http_request = std::make_shared<HttpRequest>();
  AzurePrivateKeyFetchingClientUtils::CreateHttpRequest(
      *sign_request_context.request, *http_request);
  http_request->headers = std::make_shared<core::HttpHeaders>();
  http_request->headers->insert(
      {std::string(kAuthorizationHeaderKey),
       absl::StrCat(kBearerTokenPrefix, access_token)});
  sign_request_context.response = std::move(http_request);
  sign_request_context.result = SuccessExecutionResult();
  sign_request_context.Finish();
}

ExecutionResult AzurePrivateKeyFetcherProvider::FetchPrivateKey(
    AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
        private_key_fetching_context) noexcept {
  AsyncContext<PrivateKeyFetchingRequest, HttpRequest>
      sign_http_request_context(
          private_key_fetching_context.request,
          bind(&AzurePrivateKeyFetcherProvider::SignHttpRequestCallback, this,
               private_key_fetching_context, _1),
          private_key_fetching_context);

  return SignHttpRequest(sign_http_request_context);
}

void AzurePrivateKeyFetcherProvider::SignHttpRequestCallback(
    AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
        private_key_fetching_context,
    AsyncContext<PrivateKeyFetchingRequest, HttpRequest>&
        sign_http_request_context) noexcept {
  auto execution_result = sign_http_request_context.result;
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kAzurePrivateKeyFetcherProvider,
                      private_key_fetching_context, execution_result,
                      "Failed to sign http request.");
    private_key_fetching_context.result = execution_result;
    private_key_fetching_context.Finish();
    return;
  }

  AsyncContext<HttpRequest, HttpResponse> http_client_context(
      std::move(sign_http_request_context.response),
      bind(&AzurePrivateKeyFetcherProvider::PrivateKeyFetchingCallback, this,
           private_key_fetching_context, _1),
      private_key_fetching_context);
  execution_result = http_client_->PerformRequest(http_client_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kAzurePrivateKeyFetcherProvider, private_key_fetching_context,
        execution_result,
        "Failed to perform sign http request to reach endpoint %s.",
        private_key_fetching_context.request->key_vending_endpoint
            ->private_key_vending_service_endpoint.c_str());
    private_key_fetching_context.result = execution_result;
    private_key_fetching_context.Finish();
  }
}

void AzurePrivateKeyFetcherProvider::PrivateKeyFetchingCallback(
    AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
        private_key_fetching_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context) noexcept {
  private_key_fetching_context.result = http_client_context.result;
  if (!http_client_context.result.Successful()) {
    SCP_ERROR_CONTEXT(
        kAzurePrivateKeyFetcherProvider, private_key_fetching_context,
        private_key_fetching_context.result, "Failed to fetch private key.");
    private_key_fetching_context.Finish();
    return;
  }

  if (static_cast<int>(http_client_context.response->code) == 202) {
    // `OperationDispatcher` will limit number of retry and control the amount
    // of wait before sending next request based on
    // `http_client_context.retry_count` value. Incrementing it here might not
    // be the expected usage of the field. In that case we can either:
    // - Modify `HttpClient` implementation under `http2_client/` so that it
    // retries for 202 like it already does for some other status codes
    //   (set `RetryExecutionResult()` to http_context.result).
    // - Implement a retry mechanism in this class without depending on
    // `OperationDispatcher`.
    http_client_context.retry_count++;
    auto execution_result = http_client_->PerformRequest(http_client_context);
    if (!execution_result.Successful()) {
      SCP_ERROR_CONTEXT(
          kAzurePrivateKeyFetcherProvider, private_key_fetching_context,
          execution_result,
          "Failed to perform sign http request to reach endpoint %s.",
          private_key_fetching_context.request->key_vending_endpoint
              ->private_key_vending_service_endpoint.c_str());
      private_key_fetching_context.result = execution_result;
      private_key_fetching_context.Finish();
    }
    return;
  }
  std::string resp(http_client_context.response->body.bytes->begin(),
                   http_client_context.response->body.bytes->end());

  nlohmann::json private_key_resp;
  try {
    private_key_resp = nlohmann::json::parse(resp);
  } catch (const nlohmann::json::parse_error& e) {
    std::string error_message =
        "Received http response could not be parsed into a JSON: ";
    error_message += e.what();
    SCP_ERROR_CONTEXT(kAzurePrivateKeyFetcherProvider,
                      private_key_fetching_context, http_client_context.result,
                      error_message);
    private_key_fetching_context.result = http_client_context.result;
    private_key_fetching_context.Finish();
    return;
  }
  if (!private_key_resp.contains(kWrappedKid)) {
    SCP_ERROR_CONTEXT(kAzurePrivateKeyFetcherProvider,
                      private_key_fetching_context, http_client_context.result,
                      "/key did not provide the wrappedKid property");
    private_key_fetching_context.result = http_client_context.result;
    private_key_fetching_context.Finish();
    return;
  }

  if (!private_key_resp.contains(kWrapped)) {
    SCP_ERROR_CONTEXT(kAzurePrivateKeyFetcherProvider,
                      private_key_fetching_context, http_client_context.result,
                      "/key did not provide the wrapped property");
    private_key_fetching_context.result = http_client_context.result;
    private_key_fetching_context.Finish();
    return;
  }

  std::string wrapped = private_key_resp[kWrapped];
  core::BytesBuffer buffer(wrapped);
  PrivateKeyFetchingResponse response;
  auto result =
      PrivateKeyFetchingClientUtils::ParsePrivateKey(buffer, response);
  if (!result.Successful()) {
    SCP_ERROR_CONTEXT(
        kAzurePrivateKeyFetcherProvider, private_key_fetching_context,
        private_key_fetching_context.result, "Failed to parse private key.");
    private_key_fetching_context.result = result;
    private_key_fetching_context.Finish();
    return;
  }

  private_key_fetching_context.response =
      std::make_shared<PrivateKeyFetchingResponse>(response);
  private_key_fetching_context.Finish();
}

#ifndef TEST_CPIO
std::unique_ptr<PrivateKeyFetcherProviderInterface>
PrivateKeyFetcherProviderFactory::Create(
    HttpClientInterface* http_client,
    RoleCredentialsProviderInterface* role_credentials_provider,
    AuthTokenProviderInterface* auth_token_provider) {
  return std::make_unique<AzurePrivateKeyFetcherProvider>(http_client,
                                                          auth_token_provider);
}
#endif
}  // namespace google::scp::cpio::client_providers
