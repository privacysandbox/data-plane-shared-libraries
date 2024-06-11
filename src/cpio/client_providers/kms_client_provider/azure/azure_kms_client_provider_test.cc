// Portions Copyright (c) Microsoft Corporation
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

#include "azure_kms_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "absl/synchronization/notification.h"
#include "src/core/curl_client/mock/mock_curl_client.h"
#include "src/core/interface/async_context.h"
#include "src/core/utils/base64.h"
#include "src/cpio/client_providers/auth_token_provider/mock/mock_auth_token_provider.h"
#include "src/cpio/client_providers/kms_client_provider/azure/error_codes.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::BytesBuffer;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_AZURE_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND;
using google::scp::core::errors::SC_AZURE_KMS_CLIENT_PROVIDER_KEY_ID_NOT_FOUND;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::MockCurlClient;
using google::scp::core::test::ResultIs;
using google::scp::core::utils::Base64Encode;
using google::scp::cpio::client_providers::mock::MockAuthTokenProvider;
using std::atomic;
using testing::Eq;
using testing::Pointee;

static constexpr char kServiceAccount[] = "account";
static constexpr char kWipProvider[] = "wip";
static constexpr char kKeyId[] = "keyId";
static constexpr char kCiphertext[] = "ciphertext";
static constexpr char kPlaintext[] = "plaintext";
static constexpr char kKmsUnwrapPath[] =
    "https://127.0.0.1:8000/app/unwrapKey?fmt=tink";
static constexpr char kUnwrapKeyResponse[] = R"(
{
  "wrapped": "ku3kczLacS0R8X7WiO2mNEXI319/42gccgiU6e19UDStU/+3uJsbqu8vvQ0yZmMmrPKKU1tXRzHAbXhXjbmIHQhuyXw2V1r2+YKdY2E/NnsrxB0UPwfKwRPLkG1ziNDarX9cmVTIjvtiECrxAbHGVIKvHxFxwzSjtTZzl8YoG0JXslrdYgkFjt/JlBjOEtt5YfyDcILs09eC+Hh3uUxi8J/Wylh9LCFsYo3NJD3Aln0oPPpjHtsxzNgOQJVHLczvdjDZkDTlvSpH8n5EoWt9eAbrUTBghY3qO5bi2/ZxrvaVesPa3Yi2oQIaL2brn+YGmZ7AqIdZJmQ141JnfS9SZtIAn0ONU/tOdVm3+dhLUP+Vcc3j5xsStmPThh1lWlaaDu6Z9ZW+jSd8IjN9o+9k+EHWSjgfOuTitokX6nk+v+DAHKdfGaayBzaKbrJmP+YYnSylgyAzA2mH47B6OA1jz26hmta0aJufDgDYak1lNhgS6Mobn3C30L+bfi3cl2AaCzogeK8NSTS7cX7TwQMSUvOxEaOitRsrdtXm3bfvKXuKGS/AFl+1cDNocriTESuAcsYm9cBN0W/LiN/sc3flD8VBnpOyVfdlzZ/1/RXNiOIJJTyTq6KIGlsA08q8zxWacoKyyL/KCrkJ7LUFdnBnfd+zBEJk6pAldyAhaFtQDo8="
})";

namespace google::scp::cpio::client_providers::test {

class AzureKmsClientProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    client_ = std::make_unique<AzureKmsClientProvider>(&http_client_,
                                                       &credentials_provider_);
  }

  void TearDown() override {}

  void MockGetSessionToken() {
    EXPECT_CALL(credentials_provider_, GetSessionToken)
        .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                   GetSessionTokenResponse>& context) {
          context.result = SuccessExecutionResult();
          context.response = std::make_shared<GetSessionTokenResponse>();
          context.response->session_token =
              std::make_shared<std::string>("test_token_contents");
          context.Finish();
          return context.result;
        });
  }

  MockCurlClient http_client_;
  std::unique_ptr<AzureKmsClientProvider> client_;
  MockAuthTokenProvider credentials_provider_;
};

TEST_F(AzureKmsClientProviderTest, NullKeyId) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_ciphertext(kCiphertext);

  absl::Notification condition;

  MockGetSessionToken();

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_AZURE_KMS_CLIENT_PROVIDER_KEY_ID_NOT_FOUND)));
        condition.Notify();
      });

  EXPECT_TRUE(client_->Decrypt(context).ok());
  condition.WaitForNotification();
}

TEST_F(AzureKmsClientProviderTest, EmptyKeyArn) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name("");
  kms_decrpyt_request->set_ciphertext(kCiphertext);

  absl::Notification condition;

  MockGetSessionToken();

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_AZURE_KMS_CLIENT_PROVIDER_KEY_ID_NOT_FOUND)));
        condition.Notify();
      });

  EXPECT_TRUE(client_->Decrypt(context).ok());
  condition.WaitForNotification();
}

TEST_F(AzureKmsClientProviderTest, NullCiphertext) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyId);

  absl::Notification condition;

  MockGetSessionToken();

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_AZURE_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND)));
        condition.Notify();
      });

  EXPECT_TRUE(client_->Decrypt(context).ok());
  condition.WaitForNotification();
}

TEST_F(AzureKmsClientProviderTest, EmptyCiphertext) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyId);
  kms_decrpyt_request->set_ciphertext("");

  absl::Notification condition;

  MockGetSessionToken();

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_AZURE_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND)));
        condition.Notify();
      });

  EXPECT_TRUE(client_->Decrypt(context).ok());
  condition.WaitForNotification();
}

TEST_F(AzureKmsClientProviderTest, SuccessToDecrypt) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyId);
  kms_decrpyt_request->set_ciphertext(kCiphertext);
  kms_decrpyt_request->set_account_identity(kServiceAccount);
  kms_decrpyt_request->set_gcp_wip_provider(kWipProvider);

  MockGetSessionToken();

  EXPECT_CALL(http_client_, PerformRequest).WillOnce([](auto& http_context) {
    http_context.result = SuccessExecutionResult();
    EXPECT_EQ(http_context.request->method, HttpMethod::POST);
    EXPECT_THAT(http_context.request->path, Pointee(Eq(kKmsUnwrapPath)));
    std::string payload(kUnwrapKeyResponse);

    http_context.response = std::make_shared<HttpResponse>();
    http_context.response->body = BytesBuffer(kUnwrapKeyResponse);
    http_context.Finish();
    return SuccessExecutionResult();
  });

  absl::Notification condition;
  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->plaintext(), kPlaintext);
        condition.Notify();
      });

  EXPECT_TRUE(client_->Decrypt(context).ok());

  condition.WaitForNotification();
}

TEST_F(AzureKmsClientProviderTest, FailedToDecrypt) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyId);
  kms_decrpyt_request->set_ciphertext(kCiphertext);
  kms_decrpyt_request->set_account_identity(kServiceAccount);
  kms_decrpyt_request->set_gcp_wip_provider(kWipProvider);

  MockGetSessionToken();

  EXPECT_CALL(http_client_, PerformRequest).WillOnce([](auto& http_context) {
    http_context.result = FailureExecutionResult(SC_UNKNOWN);
    http_context.Finish();
    return SuccessExecutionResult();
  });

  absl::Notification condition;
  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_UNKNOWN)));
        condition.Notify();
      });

  EXPECT_TRUE(client_->Decrypt(context).ok());
  condition.WaitForNotification();
}

TEST_F(AzureKmsClientProviderTest, FailedToGetAuthToken) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyId);
  kms_decrpyt_request->set_ciphertext(kCiphertext);
  kms_decrpyt_request->set_account_identity(kServiceAccount);
  kms_decrpyt_request->set_gcp_wip_provider(kWipProvider);

  EXPECT_CALL(credentials_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.Finish();
        return context.result;
      });

  absl::Notification condition;
  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_UNKNOWN)));
        condition.Notify();
      });

  EXPECT_FALSE(client_->Decrypt(context).ok());
  condition.WaitForNotification();
}
}  // namespace google::scp::cpio::client_providers::test
