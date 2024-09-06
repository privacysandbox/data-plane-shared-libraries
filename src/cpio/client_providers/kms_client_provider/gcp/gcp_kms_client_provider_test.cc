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

#include "src/cpio/client_providers/kms_client_provider/gcp/gcp_kms_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/synchronization/notification.h"
#include "src/core/interface/async_context.h"
#include "src/core/utils/base64.h"
#include "src/cpio/client_providers/kms_client_provider/gcp/error_codes.h"
#include "src/cpio/client_providers/kms_client_provider/gcp/gcp_kms_aead.h"
#include "src/cpio/client_providers/kms_client_provider/mock/gcp/mock_gcp_key_management_service_client.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using crypto::tink::Aead;
using crypto::tink::util::StatusOr;
using google::cloud::Status;
using google::cloud::StatusCode;
using GcsDecryptRequest = google::cloud::kms::v1::DecryptRequest;
using GcsDecryptResponse = google::cloud::kms::v1::DecryptResponse;
using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::
    SC_GCP_KMS_CLIENT_PROVIDER_BASE64_DECODING_FAILED;
using google::scp::core::errors::
    SC_GCP_KMS_CLIENT_PROVIDER_CIPHERTEXT_NOT_FOUND;
using google::scp::core::errors::SC_GCP_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED;
using google::scp::core::errors::SC_GCP_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::utils::Base64Encode;
using google::scp::cpio::client_providers::mock::
    MockGcpKeyManagementServiceClient;
using ::testing::ExplainMatchResult;
using ::testing::Return;
using ::testing::StrEq;

namespace google::scp::cpio::client_providers::test {
namespace {
constexpr std::string_view kServiceAccount = "account";
constexpr std::string_view kWipProvider = "wip";
constexpr std::string_view kKeyArn = "keyArn";
constexpr std::string_view kCiphertext = "ciphertext";
constexpr std::string_view kPlaintext = "plaintext";

class MockGcpKmsAeadProvider : public GcpKmsAeadProvider {
 public:
  MOCK_METHOD(ExecutionResultOr<std::shared_ptr<Aead>>, CreateAead,
              (std::string_view, std::string_view, std::string_view),
              (noexcept, override));
};

class GcpKmsClientProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto mock_aead_provider = std::make_unique<MockGcpKmsAeadProvider>();
    mock_aead_provider_ = mock_aead_provider.get();
    client_.emplace(std::move(mock_aead_provider));
    mock_gcp_key_management_service_client_ =
        std::make_shared<MockGcpKeyManagementServiceClient>();
    crypto::tink::util::StatusOr<std::unique_ptr<crypto::tink::Aead>>
        aead_result = GcpKmsAead::New(
            kKeyArn,
            std::dynamic_pointer_cast<GcpKeyManagementServiceClientInterface>(
                mock_gcp_key_management_service_client_));
    aead_ = std::move(*aead_result);
  }

  std::optional<GcpKmsClientProvider> client_;
  MockGcpKmsAeadProvider* mock_aead_provider_;
  std::shared_ptr<MockGcpKeyManagementServiceClient>
      mock_gcp_key_management_service_client_;
  std::shared_ptr<Aead> aead_;
};

TEST_F(GcpKmsClientProviderTest, NullKeyArn) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_ciphertext(kCiphertext);

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {});

  EXPECT_FALSE(client_->Decrypt(context).ok());
}

TEST_F(GcpKmsClientProviderTest, EmptyKeyArn) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name("");
  kms_decrpyt_request->set_ciphertext(kCiphertext);

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {});

  EXPECT_FALSE(client_->Decrypt(context).ok());
}

TEST_F(GcpKmsClientProviderTest, NullCiphertext) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyArn);

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {});

  EXPECT_FALSE(client_->Decrypt(context).ok());
}

TEST_F(GcpKmsClientProviderTest, EmptyCiphertext) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyArn);
  kms_decrpyt_request->set_ciphertext("");

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {});

  EXPECT_FALSE(client_->Decrypt(context).ok());
}

MATCHER_P(RequestMatches, req, "") {
  return ExplainMatchResult(StrEq(req.name()), arg.name(), result_listener) &&
         ExplainMatchResult(StrEq(req.ciphertext()), arg.ciphertext(),
                            result_listener) &&
         ExplainMatchResult(StrEq(req.additional_authenticated_data()),
                            arg.additional_authenticated_data(),
                            result_listener);
}

TEST_F(GcpKmsClientProviderTest, FailedToDecode) {
  EXPECT_CALL(*mock_aead_provider_,
              CreateAead(kWipProvider, kServiceAccount, kKeyArn))
      .WillOnce(Return(ExecutionResultOr<std::shared_ptr<Aead>>(aead_)));

  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyArn);
  kms_decrpyt_request->set_ciphertext("abc");
  kms_decrpyt_request->set_account_identity(kServiceAccount);
  kms_decrpyt_request->set_gcp_wip_provider(kWipProvider);

  absl::Notification condition;
  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_GCP_KMS_CLIENT_PROVIDER_BASE64_DECODING_FAILED)));
        condition.Notify();
      });

  EXPECT_FALSE(client_->Decrypt(context).ok());

  condition.WaitForNotification();
}

TEST_F(GcpKmsClientProviderTest, SuccessToDecrypt) {
  EXPECT_CALL(*mock_aead_provider_,
              CreateAead(kWipProvider, kServiceAccount, kKeyArn))
      .WillOnce(Return(ExecutionResultOr<std::shared_ptr<Aead>>(aead_)));

  std::string encoded_ciphertext;
  ASSERT_THAT(Base64Encode(kCiphertext, encoded_ciphertext), IsSuccessful());

  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyArn);
  kms_decrpyt_request->set_ciphertext(encoded_ciphertext);
  kms_decrpyt_request->set_account_identity(kServiceAccount);
  kms_decrpyt_request->set_gcp_wip_provider(kWipProvider);

  GcsDecryptRequest decrypt_request;
  decrypt_request.set_name(std::string(kKeyArn));
  decrypt_request.set_ciphertext(std::string(kCiphertext));
  GcsDecryptResponse decrypt_response;
  decrypt_response.set_plaintext(kPlaintext);
  EXPECT_CALL(*mock_gcp_key_management_service_client_,
              Decrypt(RequestMatches(decrypt_request)))
      .WillOnce(Return(decrypt_response));

  absl::Notification condition;
  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        ASSERT_SUCCESS(context.result);
        EXPECT_THAT(context.response->plaintext(), StrEq(kPlaintext));
        condition.Notify();
      });

  EXPECT_TRUE(client_->Decrypt(context).ok());

  condition.WaitForNotification();
}

TEST_F(GcpKmsClientProviderTest, FailedToDecrypt) {
  EXPECT_CALL(*mock_aead_provider_,
              CreateAead(kWipProvider, kServiceAccount, kKeyArn))
      .WillOnce(Return(ExecutionResultOr<std::shared_ptr<Aead>>(aead_)));

  std::string encoded_ciphertext;
  Base64Encode(kCiphertext, encoded_ciphertext);
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_key_resource_name(kKeyArn);
  kms_decrpyt_request->set_ciphertext(encoded_ciphertext);
  kms_decrpyt_request->set_account_identity(kServiceAccount);
  kms_decrpyt_request->set_gcp_wip_provider(kWipProvider);

  GcsDecryptRequest decrypt_request;
  decrypt_request.set_name(std::string(kKeyArn));
  decrypt_request.set_ciphertext(std::string(kCiphertext));
  GcsDecryptResponse decrypt_response;
  decrypt_response.set_plaintext(kPlaintext);
  EXPECT_CALL(*mock_gcp_key_management_service_client_,
              Decrypt(RequestMatches(decrypt_request)))
      .WillOnce(Return(Status(StatusCode::kInvalidArgument, "Invalid input")));

  absl::Notification condition;
  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_GCP_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED)));
        condition.Notify();
      });

  EXPECT_FALSE(client_->Decrypt(context).ok());

  condition.WaitForNotification();
}
}  // namespace
}  // namespace google::scp::cpio::client_providers::test
