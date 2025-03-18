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
#ifndef CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_FETCHER_PROVIDER_AZURE_AZURE_PRIVATE_KEY_FETCHER_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_FETCHER_PROVIDER_AZURE_AZURE_PRIVATE_KEY_FETCHER_PROVIDER_H_

#include <memory>
#include <string>

#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/interface/auth_token_provider_interface.h"
#include "src/cpio/client_providers/private_key_fetcher_provider/private_key_fetcher_provider.h"
#include "src/public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc PrivateKeyFetcherProviderInterface
 */
class AzurePrivateKeyFetcherProvider : public PrivateKeyFetcherProvider {
 public:
  /**
   * @brief Constructs a new GCP Private Key Fetching Client Provider object.
   *
   * @param http_client http client to issue http requests.
   * @param auth_token_provider auth token provider.
   * service.
   */
  AzurePrivateKeyFetcherProvider(
      core::HttpClientInterface* http_client,
      AuthTokenProviderInterface* auth_token_provider)
      : PrivateKeyFetcherProvider(http_client),
        auth_token_provider_(auth_token_provider) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult FetchPrivateKey(
      core::AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
          private_key_fetching_context) noexcept override;

  core::ExecutionResult SignHttpRequest(
      core::AsyncContext<PrivateKeyFetchingRequest, core::HttpRequest>&
          sign_http_request_context) noexcept override;

 protected:
  /**
   * @brief Triggered to fetch private key when http request is signed.
   *
   * @param private_key_fetching_context context to fetch private key.
   * @param sign_http_request_context context to sign http request.
   * @return core::ExecutionResult execution result.
   */
  void SignHttpRequestCallback(
      core::AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
          private_key_fetching_context,
      core::AsyncContext<PrivateKeyFetchingRequest, core::HttpRequest>&
          sign_http_request_context) noexcept;

  /**
   * @brief Triggered to parse private key when private key payload is fetched.
   *
   * @param private_key_fetching_context context to fetch private key.
   * @param http_client_context context to perform http request.
   * @return core::ExecutionResult execution result.
   */
  void PrivateKeyFetchingCallback(
      core::AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
          private_key_fetching_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_client_context) noexcept;

 private:
  /**
   * @brief Is called after auth_token_provider GetSessionToken() for session
   * token is completed
   *
   * @param sign_http_request_context the context for sign http request.
   * @param get_session_token the context of get session token.
   */
  void OnGetSessionTokenCallback(
      core::AsyncContext<PrivateKeyFetchingRequest, core::HttpRequest>&
          sign_http_request_context,
      core::AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
          get_session_token) noexcept;

  // Auth token provider.
  AuthTokenProviderInterface* auth_token_provider_;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_FETCHER_PROVIDER_AZURE_AZURE_PRIVATE_KEY_FETCHER_PROVIDER_H_
