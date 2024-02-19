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

#include "cpio/client_providers/kms_client_provider/src/azure/azure_kms_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <memory>
#include <string>

#include "core/curl_client/mock/mock_curl_client.h"
#include "core/interface/async_context.h"
#include "absl/synchronization/notification.h"
#include "core/utils/src/base64.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "cpio/client_providers/kms_client_provider/src/azure/error_codes.h"

using google::scp::core::BytesBuffer;
using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_AZURE_KMS_CLIENT_PROVIDER_KEY_ID_NOT_FOUND;
using google::scp::core::errors::SC_AZURE_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::utils::Base64Encode;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using std::atomic;
using testing::Eq;
using testing::Return;
using testing::Pointee;
using google::scp::core::test::MockCurlClient;

static constexpr char kServiceAccount[] = "account";
static constexpr char kWipProvider[] = "wip";
static constexpr char kKeyId[] = "keyId";
static constexpr char kCiphertext[] = "ciphertext";
static constexpr char kPlaintext[] = "plaintext";
static constexpr char kKmsUnwrapPath[] =
    "https://127.0.0.1:8000/app/unwrapKey?fmt=tink";

namespace google::scp::cpio::client_providers::test {

class AzureKmsClientProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    http_client_ = std::make_shared<MockCurlClient>();
    client_ = std::make_unique<AzureKmsClientProvider>(http_client_);
  }

  void TearDown() override { 

  }

  std::shared_ptr<MockCurlClient> http_client_;
  std::unique_ptr<AzureKmsClientProvider> client_;
};

TEST_F(AzureKmsClientProviderTest, NullKeyId) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_ciphertext(kCiphertext);

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {});

  EXPECT_THAT(client_->Decrypt(context),
              ResultIs(FailureExecutionResult(
                  SC_AZURE_KMS_CLIENT_PROVIDER_KEY_ID_NOT_FOUND)));
}

TEST_F(AzureKmsClientProviderTest, EmptyKeyArn) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name("");
  kms_decrpyt_request->set_ciphertext(kCiphertext);

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {});

  EXPECT_THAT(client_->Decrypt(context),
              ResultIs(FailureExecutionResult(
                  SC_AZURE_KMS_CLIENT_PROVIDER_KEY_ID_NOT_FOUND)));
}

TEST_F(AzureKmsClientProviderTest, NullCiphertext) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyId);

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {});

  EXPECT_THAT(client_->Decrypt(context),
              ResultIs(FailureExecutionResult(
                  SC_AZURE_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND)));
}

TEST_F(AzureKmsClientProviderTest, EmptyCiphertext) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyId);
  kms_decrpyt_request->set_ciphertext("");

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {});

  EXPECT_THAT(client_->Decrypt(context),
              ResultIs(FailureExecutionResult(
                  SC_AZURE_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND)));
}

TEST_F(AzureKmsClientProviderTest, SuccessToDecrypt) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyId);
  kms_decrpyt_request->set_ciphertext(kCiphertext);
  kms_decrpyt_request->set_account_identity(kServiceAccount);
  kms_decrpyt_request->set_gcp_wip_provider(kWipProvider);

  EXPECT_CALL(*http_client_, PerformRequest).WillOnce([](auto& http_context) {
    http_context.result = SuccessExecutionResult();
    EXPECT_EQ(http_context.request->method, HttpMethod::POST);
    EXPECT_THAT(http_context.request->path, Pointee(Eq(kKmsUnwrapPath)));
    std::string payload(http_context.request->body.bytes->begin(),
                    http_context.request->body.bytes->end());
    nlohmann::json json_payload = nlohmann::json::parse(payload);
    EXPECT_EQ(json_payload["wrapped"], kCiphertext);
    EXPECT_EQ(json_payload["kid"], kKeyId);
    EXPECT_TRUE(json_payload["attestation"].is_object());

    http_context.response = std::make_shared<HttpResponse>();
    http_context.response->body = BytesBuffer(kPlaintext);
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

  EXPECT_SUCCESS(client_->Decrypt(context));

  condition.WaitForNotification();
}

TEST_F(AzureKmsClientProviderTest, FailedToDecrypt) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyId);
  kms_decrpyt_request->set_ciphertext(kCiphertext);
  kms_decrpyt_request->set_account_identity(kServiceAccount);
  kms_decrpyt_request->set_gcp_wip_provider(kWipProvider);

  EXPECT_CALL(*http_client_, PerformRequest).WillOnce([](auto& http_context) {
    http_context.result = FailureExecutionResult(SC_UNKNOWN);
    http_context.Finish();
    return FailureExecutionResult(SC_UNKNOWN);
  });

  absl::Notification condition;
  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(SC_UNKNOWN)));
        condition.Notify();
      });

  EXPECT_THAT(client_->Decrypt(context), ResultIs(FailureExecutionResult(SC_UNKNOWN)));

  condition.WaitForNotification();
}
}  // namespace google::scp::cpio::client_providers::test