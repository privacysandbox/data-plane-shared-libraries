
// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/cpio/client_providers/private_key_fetcher_provider/gcp/gcp_private_key_fetcher_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "src/core/http2_client/mock/mock_http_client.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/auth_token_provider/mock/mock_auth_token_provider.h"
#include "src/cpio/client_providers/private_key_fetcher_provider/error_codes.h"
#include "src/cpio/client_providers/private_key_fetcher_provider/gcp/error_codes.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

namespace google::scp::cpio::client_providers::test {
namespace {

using google::scp::core::AsyncContext;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_GCP_PRIVATE_KEY_FETCHER_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_HTTP_CLIENT_NOT_FOUND;
using google::scp::core::http2_client::mock::MockHttpClient;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::GcpPrivateKeyFetcherProvider;
using google::scp::cpio::client_providers::mock::MockAuthTokenProvider;
using testing::Pair;
using testing::Pointee;
using testing::Return;
using testing::SetArgPointee;
using testing::UnorderedElementsAre;

constexpr std::string_view kAccountIdentity = "accountIdentity";
constexpr std::string_view kRegion = "us-east-1";
constexpr std::string_view kKeyId = "123";
constexpr std::string_view kPrivateKeyBaseUri = "http://localhost.test:8000";
constexpr std::string_view kPrivateKeyCloudfunctionUri =
    "http://cloudfunction.test:8000";
constexpr std::string_view kSessionTokenMock = "session-token-test";
constexpr std::string_view kAuthorizationHeaderKey = "Authorization";
constexpr std::string_view kBearerTokenPrefix = "Bearer ";

class GcpPrivateKeyFetcherProviderTest : public ::testing::Test {
 protected:
  GcpPrivateKeyFetcherProviderTest()
      : gcp_private_key_fetcher_provider_(std::in_place, &http_client_,
                                          &credentials_provider_) {
    EXPECT_SUCCESS(gcp_private_key_fetcher_provider_->Init());
    EXPECT_SUCCESS(gcp_private_key_fetcher_provider_->Run());

    request_ = std::make_shared<PrivateKeyFetchingRequest>();
    request_->key_id = std::make_shared<std::string>(kKeyId);
    auto endpoint = std::make_shared<PrivateKeyVendingEndpoint>();
    endpoint->gcp_private_key_vending_service_cloudfunction_url =
        kPrivateKeyCloudfunctionUri;
    endpoint->private_key_vending_service_endpoint = kPrivateKeyBaseUri;
    endpoint->service_region = kRegion;
    endpoint->account_identity = kAccountIdentity;
    request_->key_vending_endpoint = std::move(endpoint);
  }

  ~GcpPrivateKeyFetcherProviderTest() {
    if (gcp_private_key_fetcher_provider_) {
      EXPECT_SUCCESS(gcp_private_key_fetcher_provider_->Stop());
    }
  }

  void MockRequest(std::string_view uri) {
    http_client_.request_mock = HttpRequest();
    http_client_.request_mock.path = std::make_shared<std::string>(uri);
  }

  void MockResponse(std::string_view str) {
    http_client_.response_mock = HttpResponse();
    http_client_.response_mock.body = BytesBuffer(str);
  }

  MockHttpClient http_client_;
  MockAuthTokenProvider credentials_provider_;
  std::optional<GcpPrivateKeyFetcherProvider> gcp_private_key_fetcher_provider_;
  std::shared_ptr<PrivateKeyFetchingRequest> request_;
};

TEST_F(GcpPrivateKeyFetcherProviderTest, MissingHttpClient) {
  gcp_private_key_fetcher_provider_.emplace(nullptr, &credentials_provider_);

  EXPECT_THAT(gcp_private_key_fetcher_provider_->Init(),
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_FETCHER_PROVIDER_HTTP_CLIENT_NOT_FOUND)));
}

TEST_F(GcpPrivateKeyFetcherProviderTest, MissingCredentialsProvider) {
  gcp_private_key_fetcher_provider_.emplace(&http_client_, nullptr);

  EXPECT_THAT(
      gcp_private_key_fetcher_provider_->Init(),
      ResultIs(FailureExecutionResult(
          SC_GCP_PRIVATE_KEY_FETCHER_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND)));
}

MATCHER_P(TargetAudienceUriEquals, expected_target_audience_uri, "") {
  return ExplainMatchResult(*arg.request->token_target_audience_uri,
                            expected_target_audience_uri, result_listener);
}

TEST_F(GcpPrivateKeyFetcherProviderTest, SignHttpRequest) {
  absl::Notification condition;

  EXPECT_CALL(credentials_provider_,
              GetSessionTokenForTargetAudience(
                  TargetAudienceUriEquals(kPrivateKeyCloudfunctionUri)))
      .WillOnce([=](AsyncContext<GetSessionTokenForTargetAudienceRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = std::make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            std::make_shared<std::string>(kSessionTokenMock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  AsyncContext<PrivateKeyFetchingRequest, HttpRequest> context(
      request_,
      [&](AsyncContext<PrivateKeyFetchingRequest, HttpRequest>& context) {
        EXPECT_SUCCESS(context.result);
        const auto& signed_request_ = *context.response;

        EXPECT_EQ(signed_request_.method, HttpMethod::GET);
        EXPECT_THAT(signed_request_.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        condition.Notify();
        return SuccessExecutionResult();
      });

  EXPECT_THAT(gcp_private_key_fetcher_provider_->SignHttpRequest(context),
              IsSuccessful());
  condition.WaitForNotification();
}

TEST_F(GcpPrivateKeyFetcherProviderTest, FailedToGetCredentials) {
  EXPECT_CALL(credentials_provider_,
              GetSessionTokenForTargetAudience(
                  TargetAudienceUriEquals(kPrivateKeyCloudfunctionUri)))
      .WillOnce([=](AsyncContext<GetSessionTokenForTargetAudienceRequest,
                                 GetSessionTokenResponse>& context) {
        context.Finish(FailureExecutionResult(SC_UNKNOWN));
        return context.result;
      });

  absl::Notification condition;
  AsyncContext<PrivateKeyFetchingRequest, HttpRequest> context(
      request_,
      [&](AsyncContext<PrivateKeyFetchingRequest, HttpRequest>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_UNKNOWN)));
        condition.Notify();
      });

  EXPECT_THAT(gcp_private_key_fetcher_provider_->SignHttpRequest(context),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  condition.WaitForNotification();
}
}  // namespace
}  // namespace google::scp::cpio::client_providers::test
