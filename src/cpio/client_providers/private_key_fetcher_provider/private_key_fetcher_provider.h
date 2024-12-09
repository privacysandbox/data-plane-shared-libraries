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

#ifndef CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_FETCHER_PROVIDER_PRIVATE_KEY_FETCHER_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_FETCHER_PROVIDER_PRIVATE_KEY_FETCHER_PROVIDER_H_

#include <memory>
#include <string>

#include "src/core/interface/async_context.h"
#include "src/core/interface/http_client_interface.h"
#include "src/cpio/client_providers/interface/private_key_fetcher_provider_interface.h"
#include "src/public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc PrivateKeyFetcherProviderInterface
 */
class PrivateKeyFetcherProvider : public PrivateKeyFetcherProviderInterface {
 public:
  virtual ~PrivateKeyFetcherProvider() = default;

  explicit PrivateKeyFetcherProvider(
      core::HttpClientInterface* http_client,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext))
      : http_client_(http_client), log_context_(log_context) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult FetchPrivateKey(
      core::AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
          private_key_fetching_context) noexcept override;

 protected:
  /**
   * @brief Sign Http request with credentials.
   *
   * @param sign_http_request_context execution context.
   * @return core::ExecutionResult execution result.
   */
  virtual core::ExecutionResult SignHttpRequest(
      core::AsyncContext<PrivateKeyFetchingRequest, core::HttpRequest>&
          sign_http_request_context) noexcept = 0;

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

  /// HttpClient for issuing HTTP actions.
  core::HttpClientInterface* http_client_;

  // Log context for PS_VLOG and PS_LOG to enable console or otel logging
  privacy_sandbox::server_common::log::PSLogContext& log_context_ =
      const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
          privacy_sandbox::server_common::log::kNoOpContext);
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_FETCHER_PROVIDER_PRIVATE_KEY_FETCHER_PROVIDER_H_
