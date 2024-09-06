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

#include "aws_auth_token_provider.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/functional/bind_front.h"
#include "src/core/common/uuid/uuid.h"
#include "src/public/core/interface/execution_result.h"

using google::scp::core::AsyncContext;
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
    SC_AWS_INSTANCE_AUTHORIZER_PROVIDER_INITIALIZATION_FAILED;

namespace {
constexpr std::string_view kAwsAuthTokenProvider = "AwsAuthTokenProvider";

/// Use IMDSv2. The IPv4 address of the IMDSv2 is 169.254.169.254.
/// For more information, see
/// https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/configuring-instance-metadata-service.html
constexpr std::string_view kTokenServerPath =
    "http://169.254.169.254/latest/api/token";
constexpr std::string_view kTokenTtlInSecondHeader =
    "X-aws-ec2-metadata-token-ttl-seconds";
constexpr int kTokenTtlInSecondHeaderValue = 21600;

}  // namespace

namespace google::scp::cpio::client_providers {
AwsAuthTokenProvider::AwsAuthTokenProvider(
    absl::Nonnull<HttpClientInterface*> http_client)
    : http_client_(http_client) {}

ExecutionResult AwsAuthTokenProvider::GetSessionToken(
    AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context) noexcept {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->method = HttpMethod::PUT;
  http_context.request->headers = std::make_shared<HttpHeaders>();
  http_context.request->headers->insert(
      {std::string(kTokenTtlInSecondHeader),
       std::to_string(kTokenTtlInSecondHeaderValue)});

  http_context.request->path = std::make_shared<Uri>(kTokenServerPath);

  http_context.callback =
      absl::bind_front(&AwsAuthTokenProvider::OnGetSessionTokenCallback, this,
                       get_token_context);

  auto execution_result = http_client_->PerformRequest(http_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kAwsAuthTokenProvider, get_token_context,
                      execution_result,
                      "Failed to perform http request to fetch access token.");
    get_token_context.Finish(execution_result);
    return execution_result;
  }

  return SuccessExecutionResult();
}

void AwsAuthTokenProvider::OnGetSessionTokenCallback(
    AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context) noexcept {
  if (!http_client_context.result.Successful()) {
    SCP_ERROR_CONTEXT(
        kAwsAuthTokenProvider, get_token_context, http_client_context.result,
        "Failed to get access token from Instance Metadata server");
    get_token_context.Finish(http_client_context.result);

    return;
  }

  get_token_context.response = std::make_shared<GetSessionTokenResponse>();
  get_token_context.response->session_token = std::make_shared<std::string>(
      http_client_context.response->body.ToString());
  get_token_context.response->token_lifetime_in_seconds =
      std::chrono::seconds(kTokenTtlInSecondHeaderValue);
  get_token_context.Finish(SuccessExecutionResult());
}

ExecutionResult AwsAuthTokenProvider::GetSessionTokenForTargetAudience(
    AsyncContext<GetSessionTokenForTargetAudienceRequest,
                 GetSessionTokenResponse>& get_token_context) noexcept {
  // Not implemented.
  return FailureExecutionResult(SC_UNKNOWN);
}

absl::Nonnull<std::unique_ptr<AuthTokenProviderInterface>>
AuthTokenProviderFactory::Create(
    absl::Nonnull<core::HttpClientInterface*> http1_client) {
  return std::make_unique<AwsAuthTokenProvider>(http1_client);
}
}  // namespace google::scp::cpio::client_providers
