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

#include "private_key_fetcher_provider.h"

#include <memory>
#include <utility>

#include <nlohmann/json.hpp>

#include "absl/functional/bind_front.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/interface/private_key_fetcher_provider_interface.h"
#include "src/public/core/interface/execution_result.h"

#include "error_codes.h"
#include "private_key_fetcher_provider_utils.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_HTTP_CLIENT_NOT_FOUND;
using google::scp::cpio::client_providers::PrivateKeyFetchingRequest;
using google::scp::cpio::client_providers::PrivateKeyFetchingResponse;

namespace {
constexpr std::string_view kPrivateKeyFetcherProvider =
    "PrivateKeyFetcherProvider";
}

namespace google::scp::cpio::client_providers {
ExecutionResult PrivateKeyFetcherProvider::Init() noexcept {
  if (!http_client_) {
    auto execution_result = FailureExecutionResult(
        SC_PRIVATE_KEY_FETCHER_PROVIDER_HTTP_CLIENT_NOT_FOUND);
    SCP_ERROR(kPrivateKeyFetcherProvider, kZeroUuid, execution_result,
              "Failed to get http client.");
    auto error_message = google::scp::core::errors::GetErrorMessage(
        execution_result.status_code);
    PS_LOG(ERROR, log_context_)
        << "Failed to get http client. Error message: " << error_message;
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyFetcherProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyFetcherProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyFetcherProvider::FetchPrivateKey(
    AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
        private_key_fetching_context) noexcept {
  AsyncContext<PrivateKeyFetchingRequest, HttpRequest>
      sign_http_request_context(
          private_key_fetching_context.request,
          absl::bind_front(&PrivateKeyFetcherProvider::SignHttpRequestCallback,
                           this, private_key_fetching_context),
          private_key_fetching_context);

  return SignHttpRequest(sign_http_request_context);
}

void PrivateKeyFetcherProvider::SignHttpRequestCallback(
    AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
        private_key_fetching_context,
    AsyncContext<PrivateKeyFetchingRequest, HttpRequest>&
        sign_http_request_context) noexcept {
  auto execution_result = sign_http_request_context.result;
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kPrivateKeyFetcherProvider, private_key_fetching_context,
                      execution_result, "Failed to sign http request.");
    auto error_message = google::scp::core::errors::GetErrorMessage(
        execution_result.status_code);
    PS_LOG(ERROR, log_context_)
        << "Failed to sign http request. Error message: " << error_message;
    private_key_fetching_context.Finish(execution_result);
    return;
  }

  AsyncContext<HttpRequest, HttpResponse> http_client_context(
      std::move(sign_http_request_context.response),
      absl::bind_front(&PrivateKeyFetcherProvider::PrivateKeyFetchingCallback,
                       this, private_key_fetching_context),
      private_key_fetching_context);
  execution_result = http_client_->PerformRequest(http_client_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kPrivateKeyFetcherProvider, private_key_fetching_context,
        execution_result,
        "Failed to perform sign http request to reach endpoint %s.",
        private_key_fetching_context.request->key_vending_endpoint
            ->private_key_vending_service_endpoint.c_str());
    auto error_message = google::scp::core::errors::GetErrorMessage(
        execution_result.status_code);
    PS_LOG(ERROR, log_context_)
        << "Failed to perform sign http request to reach endpoint ."
        << private_key_fetching_context.request->key_vending_endpoint
               ->private_key_vending_service_endpoint.c_str()
        << ". Error message: " << error_message;
    private_key_fetching_context.Finish(execution_result);
  }
}

void PrivateKeyFetcherProvider::PrivateKeyFetchingCallback(
    AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
        private_key_fetching_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context) noexcept {
  private_key_fetching_context.result = http_client_context.result;
  if (!http_client_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kPrivateKeyFetcherProvider, private_key_fetching_context,
                      private_key_fetching_context.result,
                      "Failed to fetch private key.");
    auto error_message = google::scp::core::errors::GetErrorMessage(
        private_key_fetching_context.result.status_code);
    PS_LOG(ERROR, log_context_)
        << "Failed to fetch private key. Error message: " << error_message;
    private_key_fetching_context.Finish();
    return;
  }

  PrivateKeyFetchingResponse response;
  auto result = PrivateKeyFetchingClientUtils::ParsePrivateKey(
      http_client_context.response->body, response);
  if (!result.Successful()) {
    SCP_ERROR_CONTEXT(kPrivateKeyFetcherProvider, private_key_fetching_context,
                      result, "Failed to parse private key.");
    auto error_message =
        google::scp::core::errors::GetErrorMessage(result.status_code);
    PS_LOG(ERROR, log_context_)
        << "Failed to parse private key. Error message: " << error_message;
    private_key_fetching_context.Finish(result);
    return;
  }

  private_key_fetching_context.response =
      std::make_shared<PrivateKeyFetchingResponse>(response);
  private_key_fetching_context.Finish();
}
}  // namespace google::scp::cpio::client_providers
