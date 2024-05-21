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

#include "src/cpio/client_providers/kms_client_provider/gcp/gcp_kms_aead.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "google/cloud/kms/key_management_client.h"
#include "src/cpio/client_providers/kms_client_provider/mock/gcp/mock_gcp_key_management_service_client.h"

using crypto::tink::Aead;
using crypto::tink::util::StatusOr;
using google::cloud::Status;
using google::cloud::StatusCode;
using google::cloud::kms::v1::DecryptRequest;
using google::cloud::kms::v1::DecryptResponse;
using google::scp::cpio::client_providers::mock::
    MockGcpKeyManagementServiceClient;
using ::testing::ExplainMatchResult;
using ::testing::Return;
using ::testing::StrEq;

namespace google::scp::cpio::client_providers::test {
namespace {
constexpr std::string_view kKeyName = "test";
constexpr std::string_view kPlaintext = "plaintext";
constexpr std::string_view kCiphertext = "ciphertext";
constexpr std::string_view kAssociatedData = "data";

class GcpKmsAeadTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_gcp_kms_client_ =
        std::make_shared<MockGcpKeyManagementServiceClient>();
  }

  std::shared_ptr<MockGcpKeyManagementServiceClient> mock_gcp_kms_client_;
};

TEST_F(GcpKmsAeadTest, SuccessToCreateGcpKmsAead) {
  StatusOr<std::unique_ptr<Aead>> kms_aead = GcpKmsAead::New(
      kKeyName,
      std::dynamic_pointer_cast<GcpKeyManagementServiceClientInterface>(
          mock_gcp_kms_client_));
  ASSERT_TRUE(kms_aead.ok());
}

TEST_F(GcpKmsAeadTest, FailedToCreateGcpKmsAeadWithEmptyKeyArn) {
  StatusOr<std::unique_ptr<Aead>> kms_aead = GcpKmsAead::New(
      "", std::dynamic_pointer_cast<GcpKeyManagementServiceClientInterface>(
              mock_gcp_kms_client_));
  ASSERT_FALSE(kms_aead.ok());
  EXPECT_EQ(kms_aead.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(GcpKmsAeadTest, FailedToCreateGcpKmsAeadWithEmptyKMSClient) {
  StatusOr<std::unique_ptr<Aead>> kms_aead = GcpKmsAead::New(kKeyName, nullptr);
  ASSERT_FALSE(kms_aead.ok());
  EXPECT_EQ(kms_aead.status().code(), absl::StatusCode::kInvalidArgument);
}

MATCHER_P(RequestMatches, req, "") {
  return ExplainMatchResult(StrEq(req.name()), arg.name(), result_listener) &&
         ExplainMatchResult(StrEq(req.ciphertext()), arg.ciphertext(),
                            result_listener) &&
         ExplainMatchResult(StrEq(req.additional_authenticated_data()),
                            arg.additional_authenticated_data(),
                            result_listener);
}

TEST_F(GcpKmsAeadTest, SuccessToDecrypt) {
  DecryptRequest decrypt_request;
  decrypt_request.set_name(kKeyName);
  decrypt_request.set_ciphertext(std::string(kCiphertext));
  decrypt_request.set_additional_authenticated_data(
      std::string(kAssociatedData));
  DecryptResponse decrypt_response;
  decrypt_response.set_plaintext(kPlaintext);
  EXPECT_CALL(*mock_gcp_kms_client_, Decrypt(RequestMatches(decrypt_request)))
      .WillOnce(Return(decrypt_response));

  StatusOr<std::unique_ptr<Aead>> kms_aead = GcpKmsAead::New(
      kKeyName,
      std::dynamic_pointer_cast<GcpKeyManagementServiceClientInterface>(
          mock_gcp_kms_client_));
  StatusOr<std::string> actual_plain_text =
      (*kms_aead)->Decrypt(kCiphertext, kAssociatedData);

  ASSERT_TRUE(actual_plain_text.ok());
  EXPECT_THAT(*actual_plain_text, StrEq(kPlaintext));
}

TEST_F(GcpKmsAeadTest, FailedToDecrypt) {
  DecryptRequest req;
  req.set_name(kKeyName);
  req.set_ciphertext(std::string(kCiphertext));
  req.set_additional_authenticated_data(std::string(kAssociatedData));
  EXPECT_CALL(*mock_gcp_kms_client_, Decrypt(RequestMatches(req)))
      .WillOnce(Return(Status(StatusCode::kInvalidArgument, "Invalid input")));

  StatusOr<std::unique_ptr<Aead>> kms_aead = GcpKmsAead::New(
      kKeyName,
      std::dynamic_pointer_cast<GcpKeyManagementServiceClientInterface>(
          mock_gcp_kms_client_));
  StatusOr<std::string> actual_plain_text =
      (*kms_aead)->Decrypt(kCiphertext, kAssociatedData);

  ASSERT_FALSE(actual_plain_text.ok());
  EXPECT_EQ(actual_plain_text.status().code(),
            absl::StatusCode::kInvalidArgument);
}
}  // namespace
}  // namespace google::scp::cpio::client_providers::test
