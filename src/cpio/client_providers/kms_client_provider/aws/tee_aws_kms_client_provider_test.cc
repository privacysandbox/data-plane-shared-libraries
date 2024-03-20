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

#include "src/cpio/client_providers/kms_client_provider/aws/tee_aws_kms_client_provider.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <aws/core/Aws.h>

#include "absl/strings/str_join.h"
#include "absl/synchronization/notification.h"
#include "src/core/interface/async_context.h"
#include "src/core/utils/base64.h"
#include "src/core/utils/error_codes.h"
#include "src/cpio/client_providers/kms_client_provider/aws/tee_error_codes.h"
#include "src/cpio/client_providers/kms_client_provider/mock/aws/mock_tee_aws_kms_client_provider_with_overrides.h"
#include "src/cpio/client_providers/role_credentials_provider/mock/mock_role_credentials_provider.h"
#include "src/cpio/common/aws/error_codes.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::Utils::ByteBuffer;
using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionStatus;
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::SC_CORE_UTILS_INVALID_BASE64_ENCODING_LENGTH;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_CREDENTIAL_PROVIDER_NOT_FOUND;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::utils::Base64Encode;
using google::scp::cpio::client_providers::RoleCredentialsProviderInterface;
using google::scp::cpio::client_providers::mock::MockRoleCredentialsProvider;
using google::scp::cpio::client_providers::mock::
    MockTeeAwsKmsClientProviderWithOverrides;
using ::testing::StrEq;

namespace {
constexpr std::string_view kAssumeRoleArn = "assumeRoleArn";
constexpr std::string_view kCiphertext = "ciphertext";
constexpr std::string_view kRegion = "us-east-1";
}  // namespace

namespace google::scp::cpio::client_providers::test {
class TeeAwsKmsClientProviderTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    SDKOptions options;
    InitAPI(options);
  }

  static void TearDownTestSuite() {
    SDKOptions options;
    ShutdownAPI(options);
  }

  void SetUp() override { client_.emplace(&mock_credentials_provider_); }

  std::optional<MockTeeAwsKmsClientProviderWithOverrides> client_;
  MockRoleCredentialsProvider mock_credentials_provider_;
};

TEST_F(TeeAwsKmsClientProviderTest, SuccessToDecrypt) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_account_identity(kAssumeRoleArn);
  kms_decrpyt_request->set_kms_region(kRegion);
  kms_decrpyt_request->set_ciphertext(kCiphertext);
  absl::Notification condition;

  const std::string expect_command = absl::StrJoin(
      std::vector<std::string_view>{
          TeeAwsKmsClientProvider::kAwsNitroEnclavesCliPath,
          "--region us-east-1"
          "--aws-access-key-id access_key_id"
          "--aws-secret-access-key access_key_secret"
          "--aws-session-token security_token"
          "--ciphertext ",
          kCiphertext,
      },
      " ");

  std::string encoded_text;
  core::utils::Base64Encode(expect_command, encoded_text);
  client_->returned_plaintext = encoded_text;

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        ASSERT_SUCCESS(context.result);
        EXPECT_THAT(context.response->plaintext(), StrEq(expect_command));
        condition.Notify();
      });

  EXPECT_TRUE(client_->Decrypt(context).ok());
  condition.WaitForNotification();
}

TEST_F(TeeAwsKmsClientProviderTest, FailedToDecode) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_account_identity(kAssumeRoleArn);
  kms_decrpyt_request->set_kms_region(kRegion);
  kms_decrpyt_request->set_ciphertext(kCiphertext);
  absl::Notification condition;

  client_->returned_plaintext = "invalid";

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_CORE_UTILS_INVALID_BASE64_ENCODING_LENGTH)));
        condition.Notify();
      });

  EXPECT_TRUE(client_->Decrypt(context).ok());
  condition.WaitForNotification();
}

TEST_F(TeeAwsKmsClientProviderTest, MissingCipherText) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_account_identity(kAssumeRoleArn);
  kms_decrpyt_request->set_kms_region(kRegion);
  absl::Notification condition;

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_TEE_AWS_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND)));
        condition.Notify();
      });
  EXPECT_FALSE(client_->Decrypt(context).ok());
  condition.WaitForNotification();
}

TEST_F(TeeAwsKmsClientProviderTest, MissingAssumeRoleArn) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_kms_region(kRegion);
  kms_decrpyt_request->set_ciphertext(kCiphertext);
  absl::Notification condition;

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_TEE_AWS_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND)));
        condition.Notify();
      });
  EXPECT_FALSE(client_->Decrypt(context).ok());
  condition.WaitForNotification();
}

TEST_F(TeeAwsKmsClientProviderTest, MissingRegion) {
  auto kms_decrpyt_request = std::make_shared<DecryptRequest>();
  kms_decrpyt_request->set_account_identity(kAssumeRoleArn);
  kms_decrpyt_request->set_ciphertext(kCiphertext);
  absl::Notification condition;

  AsyncContext<DecryptRequest, DecryptResponse> context(
      kms_decrpyt_request,
      [&](AsyncContext<DecryptRequest, DecryptResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_TEE_AWS_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND)));
        condition.Notify();
      });
  EXPECT_FALSE(client_->Decrypt(context).ok());
  condition.WaitForNotification();
}
}  // namespace google::scp::cpio::client_providers::test
