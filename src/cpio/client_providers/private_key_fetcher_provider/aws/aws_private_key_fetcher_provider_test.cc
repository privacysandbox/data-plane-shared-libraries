
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

#include "src/cpio/client_providers/private_key_fetcher_provider/aws/aws_private_key_fetcher_provider.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <aws/core/Aws.h>

#include "absl/synchronization/notification.h"
#include "src/core/http2_client/mock/mock_http_client.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/type_def.h"
#include "src/cpio/client_providers/private_key_fetcher_provider/aws/error_codes.h"
#include "src/cpio/client_providers/private_key_fetcher_provider/error_codes.h"
#include "src/cpio/client_providers/role_credentials_provider/mock/mock_role_credentials_provider.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

namespace google::scp::cpio::client_providers::test {
namespace {

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using google::scp::core::AsyncContext;
using google::scp::core::AwsV4Signer;
using google::scp::core::Byte;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND;
using google::scp::core::errors::
    SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_REGION_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_HTTP_CLIENT_NOT_FOUND;
using google::scp::core::http2_client::mock::MockHttpClient;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::AwsPrivateKeyFetcherProvider;
using google::scp::cpio::client_providers::mock::MockRoleCredentialsProvider;

constexpr std::string_view kAccountIdentity = "accountIdentity";
constexpr std::string_view kRegion = "us-east-1";
constexpr std::string_view kKeyId = "123";
constexpr std::string_view kPrivateKeyBaseUri = "http://localhost.test:8000";

class AwsPrivateKeyFetcherProviderTest : public ::testing::Test {
 protected:
  AwsPrivateKeyFetcherProviderTest()
      : aws_private_key_fetcher_provider_(std::in_place, &http_client_,
                                          &credentials_provider_) {
    SDKOptions options;
    InitAPI(options);
    EXPECT_SUCCESS(aws_private_key_fetcher_provider_->Init());
    EXPECT_SUCCESS(aws_private_key_fetcher_provider_->Run());

    request_ = std::make_shared<PrivateKeyFetchingRequest>();
    request_->key_id = std::make_shared<std::string>(kKeyId);
    request_->key_vending_endpoint =
        std::make_shared<PrivateKeyVendingEndpoint>();
    request_->key_vending_endpoint->private_key_vending_service_endpoint =
        kPrivateKeyBaseUri;
    request_->key_vending_endpoint->service_region = kRegion;
    request_->key_vending_endpoint->account_identity = kAccountIdentity;
  }

  ~AwsPrivateKeyFetcherProviderTest() {
    if (aws_private_key_fetcher_provider_) {
      EXPECT_SUCCESS(aws_private_key_fetcher_provider_->Stop());
    }
    SDKOptions options;
    ShutdownAPI(options);
  }

  void MockRequest(std::string_view uri) {
    http_client_.request_mock = HttpRequest();
    http_client_.request_mock.path = std::make_shared<std::string>(uri);
  }

  void MockResponse(std::string_view str) {
    http_client_.response_mock = HttpResponse();
    http_client_.response_mock.body = std::make_shared<std::string>(str);
  }

  MockHttpClient http_client_;
  MockRoleCredentialsProvider credentials_provider_;
  std::optional<AwsPrivateKeyFetcherProvider> aws_private_key_fetcher_provider_;
  std::shared_ptr<PrivateKeyFetchingRequest> request_;
};

TEST_F(AwsPrivateKeyFetcherProviderTest, MissingHttpClient) {
  aws_private_key_fetcher_provider_.emplace(nullptr, &credentials_provider_);

  EXPECT_THAT(aws_private_key_fetcher_provider_->Init(),
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_FETCHER_PROVIDER_HTTP_CLIENT_NOT_FOUND)));
}

TEST_F(AwsPrivateKeyFetcherProviderTest, MissingCredentialsProvider) {
  aws_private_key_fetcher_provider_.emplace(&http_client_, nullptr);

  EXPECT_THAT(
      aws_private_key_fetcher_provider_->Init(),
      ResultIs(FailureExecutionResult(
          SC_AWS_PRIVATE_KEY_FETCHER_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND)));
}

TEST_F(AwsPrivateKeyFetcherProviderTest, SignHttpRequest) {
  absl::Notification condition;

  AsyncContext<PrivateKeyFetchingRequest, HttpRequest> context(
      request_,
      [&](AsyncContext<PrivateKeyFetchingRequest, HttpRequest>& context) {
        EXPECT_SUCCESS(context.result);
        condition.Notify();
        return SuccessExecutionResult();
      });

  EXPECT_THAT(aws_private_key_fetcher_provider_->SignHttpRequest(context),
              IsSuccessful());
  condition.WaitForNotification();
}

TEST_F(AwsPrivateKeyFetcherProviderTest, FailedToGetCredentials) {
  credentials_provider_.fail_credentials = true;

  absl::Notification condition;

  AsyncContext<PrivateKeyFetchingRequest, HttpRequest> context(
      request_,
      [&](AsyncContext<PrivateKeyFetchingRequest, HttpRequest>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_UNKNOWN)));
        condition.Notify();
      });

  EXPECT_THAT(aws_private_key_fetcher_provider_->SignHttpRequest(context),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  condition.WaitForNotification();
}
}  // namespace
}  // namespace google::scp::cpio::client_providers::test
