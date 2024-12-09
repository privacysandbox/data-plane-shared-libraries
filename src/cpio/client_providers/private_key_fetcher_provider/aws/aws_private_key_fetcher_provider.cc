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

#include "aws_private_key_fetcher_provider.h"

#include <utility>
#include <vector>

#include <aws/core/auth/AWSAuthSigner.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/http/standard/StandardHttpRequest.h>

#include "absl/functional/bind_front.h"
#include "absl/strings/str_cat.h"
#include "src/core/interface/http_client_interface.h"
#include "src/core/utils/http.h"
#include "src/cpio/client_providers/interface/auth_token_provider_interface.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "src/cpio/client_providers/private_key_fetcher_provider/private_key_fetcher_provider_utils.h"
#include "src/public/core/interface/execution_result.h"

#include "error_codes.h"

using Aws::Auth::AWSCredentials;
using Aws::Auth::SimpleAWSCredentialsProvider;
using Aws::Client::AWSAuthV4Signer;
using google::scp::core::AsyncContext;
using google::scp::core::AwsV4Signer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND;
using google::scp::core::errors::
    SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_FAILED_TO_GET_URI;
using google::scp::core::errors::
    SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_FAILED_TO_SIGN;
using google::scp::core::errors::
    SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_REGION_NOT_FOUND;
using google::scp::core::utils::GetEscapedUriWithQuery;

namespace {
constexpr std::string_view kAwsPrivateKeyFetcherProvider =
    "AwsPrivateKeyFetcherProvider";
/// Generic AWS service name.
constexpr std::string_view kServiceName = "execute-api";
}  // namespace

namespace google::scp::cpio::client_providers {

ExecutionResult AwsPrivateKeyFetcherProvider::Init() noexcept {
  RETURN_IF_FAILURE(PrivateKeyFetcherProvider::Init());

  if (!role_credentials_provider_) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND);
    SCP_ERROR(kAwsPrivateKeyFetcherProvider, kZeroUuid, execution_result,
              "Failed to get credentials provider.");
    auto error_message = google::scp::core::errors::GetErrorMessage(
        execution_result.status_code);
    PS_LOG(ERROR, log_context_)
        << "Failed to get credentials provider. Error message: "
        << error_message;
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult AwsPrivateKeyFetcherProvider::SignHttpRequest(
    AsyncContext<PrivateKeyFetchingRequest, HttpRequest>&
        sign_request_context) noexcept {
  auto request = std::make_shared<GetRoleCredentialsRequest>();
  request->account_identity = std::make_shared<std::string>(
      sign_request_context.request->key_vending_endpoint->account_identity);
  AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>
      get_session_credentials_context(
          std::move(request),
          absl::bind_front(
              &AwsPrivateKeyFetcherProvider::
                  CreateSessionCredentialsCallbackToSignHttpRequest,
              this, sign_request_context),
          sign_request_context);
  return role_credentials_provider_
                 ->GetRoleCredentials(get_session_credentials_context)
                 .ok()
             ? SuccessExecutionResult()
             : FailureExecutionResult(SC_UNKNOWN);
}

void AwsPrivateKeyFetcherProvider::
    CreateSessionCredentialsCallbackToSignHttpRequest(
        AsyncContext<PrivateKeyFetchingRequest, HttpRequest>&
            sign_request_context,
        AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
            get_session_credentials_context) noexcept {
  auto execution_result = get_session_credentials_context.result;
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kAwsPrivateKeyFetcherProvider, sign_request_context,
                      execution_result, "Failed to get AWS credentials.");
    auto error_message = google::scp::core::errors::GetErrorMessage(
        execution_result.status_code);
    PS_LOG(ERROR, log_context_)
        << "Failed to get AWS credentials. Error message: " << error_message;
    sign_request_context.Finish(get_session_credentials_context.result);
    return;
  }

  auto http_request = std::make_shared<HttpRequest>();
  PrivateKeyFetchingClientUtils::CreateHttpRequest(
      *sign_request_context.request, *http_request);

  execution_result = SignHttpRequestUsingV4Signer(
      http_request, *get_session_credentials_context.response->access_key_id,
      *get_session_credentials_context.response->access_key_secret,
      *get_session_credentials_context.response->security_token,
      sign_request_context.request->key_vending_endpoint->service_region);

  if (execution_result.Successful()) {
    sign_request_context.response = http_request;
  }
  sign_request_context.Finish(execution_result);
}

ExecutionResult AwsPrivateKeyFetcherProvider::SignHttpRequestUsingV4Signer(
    std::shared_ptr<HttpRequest>& http_request, std::string_view access_key,
    std::string_view secret_key, std::string_view security_token,
    std::string_view region) noexcept {
  auto credentials = AWSCredentials(access_key.data(), secret_key.data(),
                                    security_token.data());
  auto credentials_provider =
      std::make_shared<SimpleAWSCredentialsProvider>(std::move(credentials));
  auto signer = AWSAuthV4Signer(std::move(credentials_provider),
                                kServiceName.data(), region.data());

  auto path_with_query = GetEscapedUriWithQuery(*http_request);
  if (!path_with_query.Successful()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_FAILED_TO_GET_URI);
    SCP_ERROR(kAwsPrivateKeyFetcherProvider, kZeroUuid, execution_result,
              "Failed to get URI.");
    auto error_message = google::scp::core::errors::GetErrorMessage(
        execution_result.status_code);
    PS_LOG(ERROR, log_context_)
        << "Failed to get URI. Error message: " << error_message;
    return execution_result;
  }
  auto uri = Aws::Http::URI(std::move(*path_with_query));
  auto aws_request = Aws::Http::Standard::StandardHttpRequest(
      std::move(uri), Aws::Http::HttpMethod::HTTP_GET);
  if (!signer.SignRequest(aws_request)) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_FAILED_TO_SIGN);
    SCP_ERROR(kAwsPrivateKeyFetcherProvider, kZeroUuid, execution_result,
              "Failed to sign HTTP request.");
    auto error_message = google::scp::core::errors::GetErrorMessage(
        execution_result.status_code);
    PS_LOG(ERROR, log_context_)
        << "Failed to sign HTTP request. Error message: " << error_message;
    return execution_result;
  }

  http_request->headers = std::make_shared<HttpHeaders>();
  for (auto& header : aws_request.GetHeaders()) {
    http_request->headers->insert({header.first, header.second});
  }
  return SuccessExecutionResult();
}

std::unique_ptr<PrivateKeyFetcherProviderInterface>
PrivateKeyFetcherProviderFactory::Create(
    HttpClientInterface* http_client,
    RoleCredentialsProviderInterface* role_credentials_provider,
    AuthTokenProviderInterface* auth_token_provider,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  return std::make_unique<AwsPrivateKeyFetcherProvider>(
      http_client, role_credentials_provider, log_context);
}
}  // namespace google::scp::cpio::client_providers
