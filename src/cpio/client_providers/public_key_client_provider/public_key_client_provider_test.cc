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

#include "src/cpio/client_providers/public_key_client_provider/public_key_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>

#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/notification.h"
#include "src/core/http2_client/mock/mock_http_client.h"
#include "src/core/interface/async_context.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"
#include "src/public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

using google::cmrt::sdk::public_key_service::v1::ListPublicKeysRequest;
using google::cmrt::sdk::public_key_service::v1::ListPublicKeysResponse;
using google::protobuf::Any;
using google::scp::core::AsyncContext;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_PUBLIC_KEY_CLIENT_PROVIDER_ALL_URIS_REQUEST_PERFORM_FAILED;
using google::scp::core::http2_client::mock::MockHttpClient;
using google::scp::core::test::ResultIs;
using ::testing::StrEq;

namespace google::scp::cpio::client_providers::test {
namespace {
constexpr std::string_view kPublicKeyHeaderDate = "date";
constexpr std::string_view kPublicKeyHeaderCacheControl = "cache-control";
constexpr std::string_view kPrivateKeyBaseUri1 =
    "http://public_key/publicKeys1";
constexpr std::string_view kPrivateKeyBaseUri2 =
    "http://public_key/publicKeys2";
constexpr std::string_view kHeaderDateExample = "Wed, 16 Nov 2022 00:02:48 GMT";
constexpr std::string_view kCacheControlExample = "max-age=254838";
constexpr uint64_t kExpectedExpiredTimeInSeconds = 1668811806;

class PublicKeyClientProviderTestII : public ::testing::Test {
 protected:
  void SetUp() override {
    PublicKeyClientOptions public_key_client_options;
    public_key_client_options.endpoints.emplace_back(kPrivateKeyBaseUri1);
    public_key_client_options.endpoints.emplace_back(kPrivateKeyBaseUri2);

    public_key_client_.emplace(std::move(public_key_client_options),
                               &http_client_);
  }

  HttpResponse GetValidHttpResponse() {
    HttpResponse response;
    HttpHeaders headers;
    headers.insert(
        {std::string(kPublicKeyHeaderDate), std::string(kHeaderDateExample)});
    headers.insert({std::string(kPublicKeyHeaderCacheControl),
                    std::string(kCacheControlExample)});
    response.headers = std::make_shared<HttpHeaders>(headers);

    std::string bytes_str = R"({
      "keys": [
        {"id": "1234", "key": "abcdefg"},
        {"id": "5678", "key": "hijklmn"}
    ]})";

    BytesBuffer bytes(bytes_str.length());

    response.body.bytes->assign(bytes_str.begin(), bytes_str.end());
    response.body.length = bytes_str.length();
    return response;
  }

  MockHttpClient http_client_;
  std::optional<PublicKeyClientProvider> public_key_client_;
};

TEST_F(PublicKeyClientProviderTestII, ListPublicKeysSuccess) {
  absl::BlockingCounter perform_calls(2);
  auto success_response = GetValidHttpResponse();
  http_client_.perform_request_mock =
      [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
        perform_calls.DecrementCount();
        http_context.response =
            std::make_shared<HttpResponse>(success_response);
        http_context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      };

  auto request = std::make_shared<ListPublicKeysRequest>();

  absl::Notification success_callback;
  AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse> context(
      std::move(request), [&](AsyncContext<ListPublicKeysRequest,
                                           ListPublicKeysResponse>& context) {
        ASSERT_SUCCESS(context.result);
        EXPECT_THAT(context.response->public_keys()[0].key_id(), StrEq("1234"));
        EXPECT_THAT(context.response->public_keys()[0].public_key(),
                    StrEq("abcdefg"));
        EXPECT_THAT(context.response->public_keys()[1].key_id(), StrEq("5678"));
        EXPECT_THAT(context.response->public_keys()[1].public_key(),
                    StrEq("hijklmn"));
        EXPECT_EQ(context.response->expiration_time().seconds(),
                  kExpectedExpiredTimeInSeconds);
        success_callback.Notify();
      });

  EXPECT_TRUE(public_key_client_->ListPublicKeys(context).ok());
  // ListPublicKeys context callback will only run once even all uri get
  // success.
  success_callback.WaitForNotification();
  // Http client PerformRequest() has being called twice. All uris in
  // public_key_client_options will being called.
  perform_calls.Wait();
}

TEST_F(PublicKeyClientProviderTestII, ListPublicKeysFailure) {
  ExecutionResult failed_result = FailureExecutionResult(SC_UNKNOWN);

  absl::BlockingCounter perform_calls(2);
  auto success_response = GetValidHttpResponse();
  http_client_.perform_request_mock =
      [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
        perform_calls.DecrementCount();

        http_context.Finish(failed_result);
        return SuccessExecutionResult();
      };

  auto request = std::make_shared<ListPublicKeysRequest>();

  absl::Notification failure_callback;
  AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse> context(
      std::move(request), [&](AsyncContext<ListPublicKeysRequest,
                                           ListPublicKeysResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_UNKNOWN)));
        failure_callback.Notify();
      });

  EXPECT_TRUE(public_key_client_->ListPublicKeys(context).ok());

  // ListPublicKeys context callback will only run once even all uri get
  // fail.
  failure_callback.WaitForNotification();
  perform_calls.Wait();
}

TEST_F(PublicKeyClientProviderTestII, AllUrisPerformRequestFailed) {
  absl::BlockingCounter perform_calls(2);
  auto success_response = GetValidHttpResponse();
  http_client_.perform_request_mock =
      [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
        perform_calls.DecrementCount();
        return FailureExecutionResult(SC_UNKNOWN);
      };

  auto request = std::make_shared<ListPublicKeysRequest>();

  auto cpio_failure = FailureExecutionResult(
      SC_PUBLIC_KEY_CLIENT_PROVIDER_ALL_URIS_REQUEST_PERFORM_FAILED);

  absl::Notification failure_callback;
  AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse> context(
      std::move(request), [&](AsyncContext<ListPublicKeysRequest,
                                           ListPublicKeysResponse>& context) {
        EXPECT_FALSE(context.result.Successful());
        failure_callback.Notify();
      });

  EXPECT_FALSE(public_key_client_->ListPublicKeys(context).ok());

  // ListPublicKeys context callback will only run once even all uri get
  // fail.
  failure_callback.WaitForNotification();
  perform_calls.Wait();
}

TEST_F(PublicKeyClientProviderTestII, ListPublicKeysPartialUriSuccess) {
  ExecutionResult failed_result = FailureExecutionResult(SC_UNKNOWN);
  absl::BlockingCounter perform_calls(2);
  auto success_response = GetValidHttpResponse();
  http_client_.perform_request_mock =
      [&](AsyncContext<HttpRequest, HttpResponse>& http_context) {
        perform_calls.DecrementCount();
        if (*http_context.request->path == kPrivateKeyBaseUri2) {
          http_context.response =
              std::make_shared<HttpResponse>(success_response);
          http_context.Finish(SuccessExecutionResult());
          return SuccessExecutionResult();
        }
        http_context.Finish(failed_result);

        return failed_result;
      };

  auto request = std::make_shared<ListPublicKeysRequest>();
  absl::Notification success_callback;
  AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse> context(
      std::move(request), [&](AsyncContext<ListPublicKeysRequest,
                                           ListPublicKeysResponse>& context) {
        EXPECT_SUCCESS(context.result);
        success_callback.Notify();
      });

  EXPECT_TRUE(public_key_client_->ListPublicKeys(context).ok());
  // ListPublicKeys success with partial uris got success response.
  success_callback.WaitForNotification();
  perform_calls.Wait();
}
}  // namespace
}  // namespace google::scp::cpio::client_providers::test
