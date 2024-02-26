
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

#include "src/cpio/client_providers/private_key_fetcher_provider/private_key_fetcher_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/synchronization/notification.h"
#include "src/core/http2_client/mock/mock_http_client.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/private_key_fetcher_provider/error_codes.h"
#include "src/cpio/client_providers/private_key_fetcher_provider/mock/mock_private_key_fetcher_provider_with_overrides.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_HTTP_CLIENT_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_KEY_DATA_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_RESOURCE_NAME_NOT_FOUND;
using google::scp::core::http2_client::mock::MockHttpClient;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::PrivateKeyFetchingRequest;
using google::scp::cpio::client_providers::PrivateKeyFetchingResponse;
using google::scp::cpio::client_providers::mock::
    MockPrivateKeyFetcherProviderWithOverrides;
using ::testing::StrEq;

namespace google::scp::cpio::client_providers::test {
namespace {
constexpr std::string_view kKeyId = "123";
constexpr std::string_view kRegion = "region";
constexpr std::string_view kPrivateKeyBaseUri =
    "http://private_key/privateKeys";

class PrivateKeyFetcherProviderTest : public ::testing::Test {
 protected:
  PrivateKeyFetcherProviderTest()
      : private_key_fetcher_provider_(std::in_place, &http_client_) {
    private_key_fetcher_provider_->signed_http_request_mock->path =
        std::make_shared<std::string>(std::string(kPrivateKeyBaseUri) + "/" +
                                      std::string(kKeyId));
    EXPECT_SUCCESS(private_key_fetcher_provider_->Init());
    EXPECT_SUCCESS(private_key_fetcher_provider_->Run());
    request_ = std::make_shared<PrivateKeyFetchingRequest>();
    request_->key_id = std::make_shared<std::string>(kKeyId);
    request_->key_vending_endpoint =
        std::make_shared<PrivateKeyVendingEndpoint>();
    request_->key_vending_endpoint->private_key_vending_service_endpoint =
        kPrivateKeyBaseUri;
    request_->key_vending_endpoint->service_region = kRegion;
  }

  ~PrivateKeyFetcherProviderTest() {
    EXPECT_SUCCESS(private_key_fetcher_provider_->Stop());
  }

  void MockRequest(std::string_view uri) {
    http_client_.request_mock = HttpRequest();
    http_client_.request_mock.path = std::make_shared<std::string>(uri);
  }

  void MockResponse(std::string_view str) {
    BytesBuffer bytes_buffer(sizeof(str));
    bytes_buffer.bytes =
        std::make_shared<std::vector<Byte>>(str.begin(), str.end());
    bytes_buffer.capacity = sizeof(str);

    http_client_.response_mock = HttpResponse();
    http_client_.response_mock.body = std::move(bytes_buffer);
  }

  MockHttpClient http_client_;
  std::optional<MockPrivateKeyFetcherProviderWithOverrides>
      private_key_fetcher_provider_;
  std::shared_ptr<PrivateKeyFetchingRequest> request_;
};

TEST_F(PrivateKeyFetcherProviderTest, MissingHttpClient) {
  private_key_fetcher_provider_.emplace(nullptr);

  EXPECT_THAT(private_key_fetcher_provider_->Init(),
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_FETCHER_PROVIDER_HTTP_CLIENT_NOT_FOUND)));
}

TEST_F(PrivateKeyFetcherProviderTest, FetchPrivateKey) {
  MockRequest(absl::StrCat(kPrivateKeyBaseUri, "/", kKeyId));
  MockResponse(
      R"({
    "name": "encryptionKeys/123456",
    "encryptionKeyType": "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT",
    "publicKeysetHandle": "primaryKeyId",
    "publicKeyMaterial": "testtest",
    "creationTime": "1669252790485",
    "expirationTime": "1669943990485",
    "ttlTime": 0,
    "keyData": [
        {
            "publicKeySignature": "",
            "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:1234567:key",
            "keyMaterial": "test=test"
        },
        {
            "publicKeySignature": "",
            "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:12345:key",
            "keyMaterial": ""
        }
    ]
  })");

  absl::Notification condition;

  AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse> context(
      request_, [&](AsyncContext<PrivateKeyFetchingRequest,
                                 PrivateKeyFetchingResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->encryption_keys.size(), 1);
        const auto& encryption_key = *context.response->encryption_keys.begin();
        EXPECT_THAT(*encryption_key->resource_name,
                    StrEq("encryptionKeys/123456"));

        condition.Notify();
        return SuccessExecutionResult();
      });
  EXPECT_THAT(private_key_fetcher_provider_->FetchPrivateKey(context),
              IsSuccessful());
  condition.WaitForNotification();
}

TEST_F(PrivateKeyFetcherProviderTest, FailedToFetchPrivateKey) {
  ExecutionResult result = FailureExecutionResult(SC_UNKNOWN);
  http_client_.http_get_result_mock = result;

  absl::Notification condition;
  AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse> context(
      std::move(request_),
      [&](AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
              context) {
        condition.Notify();
        EXPECT_THAT(context.result, ResultIs(result));
      });
  EXPECT_THAT(private_key_fetcher_provider_->FetchPrivateKey(context),
              IsSuccessful());
  condition.WaitForNotification();
}

TEST_F(PrivateKeyFetcherProviderTest, FailedToSignHttpRequest) {
  ExecutionResult result = FailureExecutionResult(SC_UNKNOWN);
  private_key_fetcher_provider_->sign_http_request_result_mock = result;

  absl::Notification condition;
  AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse> context(
      std::move(request_),
      [&](AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
              context) {
        condition.Notify();
        EXPECT_THAT(context.result, ResultIs(result));
      });
  EXPECT_THAT(private_key_fetcher_provider_->FetchPrivateKey(context),
              ResultIs(result));
  condition.WaitForNotification();
}

TEST_F(PrivateKeyFetcherProviderTest, PrivateKeyNotFound) {
  MockRequest(absl::StrCat(kPrivateKeyBaseUri, "/", kKeyId));
  MockResponse(
      R"({
        "name": "encryptionKeys/123456",
        "encryptionKeyType": "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT",
        "publicKeysetHandle": "primaryKeyId",
        "publicKeyMaterial": "testtest",
        "creationTime": "1669252790485",
        "expirationTime": "1669943990485",
        "ttlTime": 0
    })");

  absl::Notification condition;
  AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse> context(
      std::move(request_),
      [&](AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
              context) {
        condition.Notify();
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_PRIVATE_KEY_FETCHER_PROVIDER_KEY_DATA_NOT_FOUND)));
      });
  EXPECT_THAT(private_key_fetcher_provider_->FetchPrivateKey(context),
              IsSuccessful());
  condition.WaitForNotification();
}
}  // namespace
}  // namespace google::scp::cpio::client_providers::test
