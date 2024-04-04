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

#include "src/cpio/client_providers/kms_client_provider/aws/aws_kms_aead.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <aws/core/Aws.h>

#include "src/core/interface/type_def.h"
#include "src/cpio/client_providers/kms_client_provider/mock/aws/mock_kms_client.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::KMS::KMSClient;
using Aws::KMS::Model::DecryptOutcome;
using Aws::KMS::Model::DecryptRequest;
using Aws::KMS::Model::DecryptResult;
using Aws::KMS::Model::EncryptOutcome;
using Aws::KMS::Model::EncryptRequest;
using Aws::KMS::Model::EncryptResult;
using crypto::tink::Aead;
using crypto::tink::util::StatusOr;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::cpio::client_providers::mock::MockKMSClient;
using ::testing::StrEq;

namespace google::scp::cpio::client_providers::test {
class AwsKmsAeadTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    SDKOptions options;
    InitAPI(options);
  }

  static void TearDownTestSuite() {
    SDKOptions options;
    ShutdownAPI(options);
  }

  void SetUp() override {
    key_arn_ = "test";
    kms_client_ = std::make_shared<MockKMSClient>();
    plain_text_ = "text";
    associated_data_ = "data";
    cipher_text_ = "cipher text";

    // Mocks EncryptRequest.
    EncryptRequest encrypt_request;
    encrypt_request.SetKeyId(key_arn_.c_str());
    Aws::Utils::ByteBuffer plaintext_buffer(
        reinterpret_cast<const unsigned char*>(plain_text_.data()),
        plain_text_.length());
    encrypt_request.SetPlaintext(plaintext_buffer);

    // Mocks success EncryptRequestOutcome.
    EncryptResult encrypt_result;
    encrypt_result.SetKeyId(key_arn_.c_str());
    Aws::Utils::ByteBuffer cipher_text_blob(
        reinterpret_cast<const unsigned char*>(cipher_text_.data()),
        cipher_text_.length());
    encrypt_result.SetCiphertextBlob(cipher_text_blob);
    EncryptOutcome encrypt_outcome(encrypt_result);

    // Mocks DecryptRequest.
    DecryptRequest decrypt_request;
    decrypt_request.SetKeyId(key_arn_.c_str());
    Aws::Utils::ByteBuffer ciphertext_buffer(
        reinterpret_cast<const unsigned char*>(cipher_text_.data()),
        cipher_text_.length());
    decrypt_request.SetCiphertextBlob(ciphertext_buffer);

    // Mocks success DecryptRequestOutcome.
    DecryptResult decrypt_result;
    decrypt_result.SetKeyId(key_arn_.c_str());
    Aws::Utils::ByteBuffer plaintext(
        reinterpret_cast<const unsigned char*>(plain_text_.data()),
        plain_text_.length());
    decrypt_result.SetPlaintext(plaintext);
    DecryptOutcome decrypt_outcome(decrypt_result);

    kms_client_->encrypt_request_mock = encrypt_request;
    kms_client_->encrypt_outcome_mock = encrypt_outcome;
    kms_client_->decrypt_request_mock = decrypt_request;
    kms_client_->decrypt_outcome_mock = decrypt_outcome;
  }

  std::string key_arn_;
  std::string plain_text_;
  std::string associated_data_;
  std::string cipher_text_;

  std::shared_ptr<MockKMSClient> kms_client_;
};

TEST_F(AwsKmsAeadTest, SuccessToCreateAwsKmsAead) {
  StatusOr<std::unique_ptr<Aead>> kms_aead =
      AwsKmsAead::New(key_arn_, kms_client_);
  ASSERT_TRUE(kms_aead.ok());
}

TEST_F(AwsKmsAeadTest, FailedToCreateAwsKmsAeadWithEmptyKeyArn) {
  StatusOr<std::unique_ptr<Aead>> kms_aead = AwsKmsAead::New("", kms_client_);
  ASSERT_FALSE(kms_aead.ok());
  EXPECT_EQ(kms_aead.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(AwsKmsAeadTest, FailedToCreateAwsKmsAeadWithEmptyKMSClient) {
  StatusOr<std::unique_ptr<Aead>> kms_aead = AwsKmsAead::New(key_arn_, nullptr);
  ASSERT_FALSE(kms_aead.ok());
  EXPECT_EQ(kms_aead.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(AwsKmsAeadTest, SuccessToEncrypt) {
  StatusOr<std::unique_ptr<Aead>> kms_aead =
      AwsKmsAead::New(key_arn_, kms_client_);
  StatusOr<std::string> actual_cipher_text =
      (*kms_aead)->Encrypt(plain_text_, associated_data_);
  ASSERT_TRUE(actual_cipher_text.ok());
  EXPECT_THAT(*actual_cipher_text, StrEq(cipher_text_));
}

TEST_F(AwsKmsAeadTest, FailedToEncrypt) {
  StatusOr<std::unique_ptr<Aead>> kms_aead =
      AwsKmsAead::New(key_arn_, kms_client_);
  std::string bad_plain_text = "bad text";
  StatusOr<std::string> actual_cipher_text =
      (*kms_aead)->Encrypt(bad_plain_text, associated_data_);
  ASSERT_FALSE(actual_cipher_text.ok());
  EXPECT_EQ(actual_cipher_text.status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST_F(AwsKmsAeadTest, SuccessToDecrypt) {
  StatusOr<std::unique_ptr<Aead>> kms_aead =
      AwsKmsAead::New(key_arn_, kms_client_);
  StatusOr<std::string> actual_plain_text =
      (*kms_aead)->Decrypt(cipher_text_, associated_data_);
  ASSERT_TRUE(actual_plain_text.ok());
  EXPECT_THAT(*actual_plain_text, StrEq(plain_text_));
}

TEST_F(AwsKmsAeadTest, FailedToDecrypt) {
  StatusOr<std::unique_ptr<Aead>> kms_aead =
      AwsKmsAead::New(key_arn_, kms_client_);
  std::string bad_cipher_text = "bad text";
  StatusOr<std::string> actual_plain_text =
      (*kms_aead)->Decrypt(bad_cipher_text, associated_data_);
  ASSERT_FALSE(actual_plain_text.ok());
  EXPECT_EQ(actual_plain_text.status().code(),
            absl::StatusCode::kInvalidArgument);
}

}  // namespace google::scp::cpio::client_providers::test
