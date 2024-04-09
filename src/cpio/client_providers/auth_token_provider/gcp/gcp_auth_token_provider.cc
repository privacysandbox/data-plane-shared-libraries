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

#include "gcp_auth_token_provider.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/base/nullability.h"
#include "absl/functional/bind_front.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/utils/base64.h"
#include "src/public/core/interface/execution_result.h"

#include "error_codes.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_BAD_SESSION_TOKEN;
using google::scp::core::errors::
    SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_INITIALIZATION_FAILED;
using google::scp::core::utils::Base64Decode;
using google::scp::core::utils::PadBase64Encoding;
using nlohmann::json;

namespace {
constexpr std::string_view kGcpAuthTokenProvider = "GcpAuthTokenProvider";

// This is not HTTPS but this is still safe according to the docs:
// https://cloud.google.com/compute/docs/metadata/overview#metadata_security_considerations
constexpr std::string_view kTokenServerPath =
    "http://metadata.google.internal/computeMetadata/v1/instance/"
    "service-accounts/default/token";
constexpr std::string_view kIdentityServerPath =
    "http://metadata/computeMetadata/v1/instance/service-accounts/default/"
    "identity";
constexpr std::string_view kMetadataFlavorHeader = "Metadata-Flavor";
constexpr std::string_view kMetadataFlavorHeaderValue = "Google";
constexpr std::string_view kJsonAccessTokenKey = "access_token";
constexpr std::string_view kJsonTokenExpiryKey = "expires_in";
constexpr std::string_view kJsonTokenTypeKey = "token_type";
constexpr std::string_view kAudienceParameter = "audience=";
constexpr std::string_view kFormatFullParameter = "format=full";

constexpr size_t kExpectedTokenPartsSize = 3;
constexpr std::string_view kJsonTokenIssuerKey = "iss";
constexpr std::string_view kJsonTokenAudienceKey = "aud";
constexpr std::string_view kJsonTokenSubjectKey = "sub";
constexpr std::string_view kJsonTokenIssuedAtKey = "iat";
constexpr std::string_view kJsonTokenExpiryKeyForTargetAudience = "exp";

// Returns a pair of iterators - one to the beginning, one to the end.
const auto& GetRequiredJWTComponents() {
  static char const* components[3];
  using iterator_type = decltype(std::cbegin(components));
  static std::pair<iterator_type, iterator_type> iterator_pair = []() {
    components[0] = kJsonAccessTokenKey.data();
    components[1] = kJsonTokenExpiryKey.data();
    components[2] = kJsonTokenTypeKey.data();
    return std::make_pair(std::cbegin(components), std::cend(components));
  }();
  return iterator_pair;
}

// Returns a pair of iterators - one to the beginning, one to the end.
const auto& GetRequiredJWTComponentsForTargetAudienceToken() {
  static char const* components[5];
  using iterator_type = decltype(std::cbegin(components));
  static std::pair<iterator_type, iterator_type> iterator_pair = []() {
    components[0] = kJsonTokenIssuerKey.data();
    components[1] = kJsonTokenAudienceKey.data();
    components[2] = kJsonTokenSubjectKey.data();
    components[3] = kJsonTokenIssuedAtKey.data();
    components[4] = kJsonTokenExpiryKeyForTargetAudience.data();
    return std::make_pair(std::cbegin(components), std::cend(components));
  }();
  return iterator_pair;
}
}  // namespace

namespace google::scp::cpio::client_providers {
GcpAuthTokenProvider::GcpAuthTokenProvider(
    absl::Nonnull<HttpClientInterface*> http_client)
    : http_client_(http_client) {}

ExecutionResult GcpAuthTokenProvider::GetSessionToken(
    AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context) noexcept {
  // Make a request to the metadata server:
  // The Application is running on a GCP VM which runs as a service account.
  // Services which run on GCP also spin up a local metadata server which can be
  // queried for details about the system. curl -H "Metadata-Flavor: Google"
  // 'http://metadata.google.internal/computeMetadata/v1/instance/service-accounts/default/token?scopes=SCOPES'
  // NOTE: Without scope setting, the access token will being assigned with full
  // access permission of current instance.
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();

  http_context.request->headers = std::make_shared<HttpHeaders>();
  http_context.request->headers->insert(
      {std::string(kMetadataFlavorHeader),
       std::string(kMetadataFlavorHeaderValue)});

  http_context.request->path = std::make_shared<Uri>(kTokenServerPath);

  http_context.callback =
      absl::bind_front(&GcpAuthTokenProvider::OnGetSessionTokenCallback, this,
                       get_token_context);

  auto execution_result = http_client_->PerformRequest(http_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpAuthTokenProvider, get_token_context,
                      execution_result,
                      "Failed to perform http request to fetch access token.");

    get_token_context.Finish(execution_result);
    return execution_result;
  }

  return SuccessExecutionResult();
}

void GcpAuthTokenProvider::OnGetSessionTokenCallback(
    AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context) noexcept {
  if (!http_client_context.result.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpAuthTokenProvider, get_token_context, http_client_context.result,
        "Failed to get access token from Instance Metadata server");

    get_token_context.Finish(http_client_context.result);
    return;
  }

  json json_response;
  try {
    json_response =
        json::parse(http_client_context.response->body.bytes->begin(),
                    http_client_context.response->body.bytes->end());
  } catch (...) {
    auto result = RetryExecutionResult(
        SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_BAD_SESSION_TOKEN);
    SCP_ERROR_CONTEXT(
        kGcpAuthTokenProvider, get_token_context, result,
        "Received http response could not be parsed into a JSON.");
    get_token_context.Finish(result);
    return;
  }

  if (!std::all_of(GetRequiredJWTComponents().first,
                   GetRequiredJWTComponents().second,
                   [&json_response](const char* const component) {
                     return json_response.contains(component);
                   })) {
    auto result = RetryExecutionResult(
        SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_BAD_SESSION_TOKEN);
    SCP_ERROR_CONTEXT(
        kGcpAuthTokenProvider, get_token_context, result,
        "Received http response does not contain all the necessary fields.");
    get_token_context.Finish(result);
    return;
  }

  get_token_context.response = std::make_shared<GetSessionTokenResponse>();

  // The life time of GCP access token is about 1 hour.
  uint64_t expiry_seconds = json_response[kJsonTokenExpiryKey].get<uint64_t>();
  get_token_context.response->token_lifetime_in_seconds =
      std::chrono::seconds(expiry_seconds);
  auto access_token = json_response[kJsonAccessTokenKey].get<std::string>();
  get_token_context.response->session_token =
      std::make_shared<std::string>(std::move(access_token));

  get_token_context.Finish(SuccessExecutionResult());
}

ExecutionResult GcpAuthTokenProvider::GetSessionTokenForTargetAudience(
    AsyncContext<GetSessionTokenForTargetAudienceRequest,
                 GetSessionTokenResponse>& get_token_context) noexcept {
  // Make a request to the metadata server:
  // The PBS is running on a GCP VM which runs as a service account. Services
  // which run on GCP also spin up a local metadata server which can be
  // queried for details about the system. curl -H "Metadata-Flavor: Google"
  // 'http://metadata/computeMetadata/v1/instance/service-accounts/default/identity?audience=AUDIENCE'

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();

  http_context.request->headers = std::make_shared<HttpHeaders>();
  http_context.request->headers->insert(
      {std::string(kMetadataFlavorHeader),
       std::string(kMetadataFlavorHeaderValue)});

  http_context.request->path = std::make_shared<Uri>(kIdentityServerPath);
  http_context.request->query = std::make_shared<std::string>(absl::StrCat(
      kAudienceParameter, *get_token_context.request->token_target_audience_uri,
      "&", kFormatFullParameter));

  http_context.callback = absl::bind_front(
      &GcpAuthTokenProvider::OnGetSessionTokenForTargetAudienceCallback, this,
      get_token_context);

  return http_client_->PerformRequest(http_context);
}

void GcpAuthTokenProvider::OnGetSessionTokenForTargetAudienceCallback(
    AsyncContext<GetSessionTokenForTargetAudienceRequest,
                 GetSessionTokenResponse>& get_token_context,
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  if (!http_context.result.Successful()) {
    get_token_context.Finish(http_context.result);
    return;
  }
  const auto& response_body = http_context.response->body.ToString();
  std::vector<std::string> token_parts = absl::StrSplit(response_body, '.');
  if (token_parts.size() != kExpectedTokenPartsSize) {
    auto result = RetryExecutionResult(
        SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_BAD_SESSION_TOKEN);
    SCP_ERROR_CONTEXT(kGcpAuthTokenProvider, get_token_context, result,
                      "Received token does not have %d parts.",
                      kExpectedTokenPartsSize);
    get_token_context.Finish(result);
    return;
  }

  // The JSON Web Token (JWT) lives in the middle (1) part of the whole
  // string.
  auto padded_jwt_or = PadBase64Encoding(token_parts[1]);
  if (!padded_jwt_or.Successful()) {
    SCP_ERROR_CONTEXT(kGcpAuthTokenProvider, get_token_context,
                      padded_jwt_or.result(),
                      "Received JWT cannot be padded correctly.");
    get_token_context.Finish(padded_jwt_or.result());
    return;
  }
  std::string decoded_json_str;
  if (auto decode_result = Base64Decode(*padded_jwt_or, decoded_json_str);
      !decode_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpAuthTokenProvider, get_token_context, decode_result,
                      "Received token JWT could not be decoded.");
    get_token_context.Finish(decode_result);
    return;
  }

  json json_web_token;
  try {
    json_web_token = json::parse(decoded_json_str);
  } catch (...) {
    auto result = RetryExecutionResult(
        SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_BAD_SESSION_TOKEN);
    SCP_ERROR_CONTEXT(kGcpAuthTokenProvider, get_token_context, result,
                      "Received JWT could not be parsed into a JSON.");
    get_token_context.Finish(result);
    return;
  }

  if (!std::all_of(GetRequiredJWTComponentsForTargetAudienceToken().first,
                   GetRequiredJWTComponentsForTargetAudienceToken().second,
                   [&json_web_token](const char* const component) {
                     return json_web_token.contains(component);
                   })) {
    auto result = RetryExecutionResult(
        SC_GCP_INSTANCE_AUTHORIZER_PROVIDER_BAD_SESSION_TOKEN);
    SCP_ERROR_CONTEXT(
        kGcpAuthTokenProvider, get_token_context, result,
        "Received JWT does not contain all the necessary fields.");
    get_token_context.Finish(result);
    return;
  }

  get_token_context.response = std::make_shared<GetSessionTokenResponse>();
  get_token_context.response->session_token =
      std::make_shared<std::string>(response_body);

  // We make an assumption that the obtaining token is instantaneous since the
  // token is fetched from the GCP platform close to the VM where this code will
  // run, but in some worst case situations, this token could take longer to
  // obtain and in those cases we will deem the token to be valid for more
  // number of seconds than it is intended to be used for, causing callers to
  // have an expired token for a small time.
  uint64_t expiry_seconds =
      json_web_token[kJsonTokenExpiryKeyForTargetAudience].get<uint64_t>();
  uint64_t issued_seconds =
      json_web_token[kJsonTokenIssuedAtKey].get<uint64_t>();
  get_token_context.response->token_lifetime_in_seconds =
      std::chrono::seconds(expiry_seconds - issued_seconds);

  get_token_context.Finish(SuccessExecutionResult());
}

absl::Nonnull<std::unique_ptr<AuthTokenProviderInterface>>
AuthTokenProviderFactory::Create(
    absl::Nonnull<core::HttpClientInterface*> http1_client) {
  return std::make_unique<GcpAuthTokenProvider>(http1_client);
}
}  // namespace google::scp::cpio::client_providers
