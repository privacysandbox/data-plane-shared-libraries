/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cpio/client_providers/private_key_client_provider/src/private_key_client_utils.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/util/time_util.h>

#include "absl/strings/escaping.h"
#include "core/interface/http_types.h"
#include "core/test/utils/timestamp_test_utils.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::private_key_service::v1::PrivateKey;
using google::protobuf::util::TimeUtil;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::
    SC_PRIVATE_KEY_CLIENT_PROVIDER_CANNOT_READ_ENCRYPTED_KEY_SET;
using google::scp::core::errors::
    SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_KEY_DATA_COUNT;
using google::scp::core::errors::
    SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_KEY_RESOURCE_NAME;
using google::scp::core::errors::
    SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_VENDING_ENDPOINT_COUNT;
using google::scp::core::errors::
    SC_PRIVATE_KEY_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_CLIENT_PROVIDER_SECRET_PIECE_SIZE_UNMATCHED;
using google::scp::core::test::ExpectTimestampEquals;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::KeyData;
using google::scp::cpio::client_providers::PrivateKeyFetchingResponse;
using std::make_shared;
using std::shared_ptr;

namespace {
constexpr char kTestKeyId[] = "name_test";
constexpr char kTestResourceName[] = "encryptionKeys/name_test";
constexpr char kTestPublicKeysetHandle[] = "publicKeysetHandle";
constexpr char kTestPublicKeyMaterial[] = "publicKeyMaterial";
constexpr int kTestExpirationTime = 123456;
constexpr int kTestCreationTime = 111111;
constexpr char kTestPublicKeySignature[] = "publicKeySignature";
constexpr char kTestKeyEncryptionKeyUriWithPrefix[] =
    "1234567890keyEncryptionKeyUri";
constexpr char kTestKeyEncryptionKeyUri[] = "keyEncryptionKeyUri";
constexpr char kTestKeyMaterial[] = "keyMaterial";
constexpr char kSinglePartyKeyMaterialJson[] =
    R"(
    {
    "keysetInfo": {
        "primaryKeyId": 1353288376,
        "keyInfo": [{
            "typeUrl": "type.googleapis.com/google.crypto.tink.EciesAeadHkdfPrivateKey",
            "outputPrefixType": "TINK",
            "keyId": 1353288376,
            "status": "ENABLED"
        }]
    },
    "encryptedKeyset": "singlepartykey"
    }
    )";
}  // namespace

namespace google::scp::cpio::client_providers::test {
shared_ptr<EncryptionKey> CreateEncryptionKeyBase() {
  auto encryption_key = make_shared<EncryptionKey>();
  encryption_key->key_id = make_shared<std::string>(kTestKeyId);
  encryption_key->resource_name = make_shared<std::string>(kTestResourceName);
  encryption_key->expiration_time_in_ms = kTestExpirationTime;
  encryption_key->creation_time_in_ms = kTestCreationTime;
  encryption_key->public_key_material =
      make_shared<std::string>(kTestPublicKeyMaterial);
  encryption_key->public_keyset_handle =
      make_shared<std::string>(kTestPublicKeysetHandle);
  return encryption_key;
}

shared_ptr<EncryptionKey> CreateEncryptionKey(
    const std::string& key_resource_name = kTestKeyEncryptionKeyUriWithPrefix) {
  auto encryption_key = CreateEncryptionKeyBase();
  encryption_key->encryption_key_type =
      EncryptionKeyType::kMultiPartyHybridEvenKeysplit;
  auto key_data = make_shared<KeyData>();
  key_data->key_encryption_key_uri =
      make_shared<std::string>(key_resource_name);
  key_data->key_material = make_shared<std::string>(kTestKeyMaterial);
  key_data->public_key_signature =
      make_shared<std::string>(kTestPublicKeySignature);
  encryption_key->key_data.emplace_back(key_data);
  return encryption_key;
}

TEST(PrivateKeyClientUtilsTest, GetKmsDecryptRequestSuccess) {
  auto encryption_key = CreateEncryptionKey();
  DecryptRequest kms_decrypt_request;
  auto result = PrivateKeyClientUtils::GetKmsDecryptRequest(
      1, encryption_key, kms_decrypt_request);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(kms_decrypt_request.ciphertext(), kTestKeyMaterial);
  EXPECT_EQ(kms_decrypt_request.key_resource_name(), kTestKeyEncryptionKeyUri);
}

TEST(PrivateKeyClientUtilsTest, GetKmsDecryptRequestFailed) {
  auto encryption_key = CreateEncryptionKey();

  auto key_data = make_shared<KeyData>();
  key_data->key_encryption_key_uri = make_shared<std::string>("");
  key_data->key_material = make_shared<std::string>("");
  key_data->public_key_signature = make_shared<std::string>("");
  encryption_key->key_data = std::vector<shared_ptr<KeyData>>({key_data});

  DecryptRequest kms_decrypt_request;
  auto result = PrivateKeyClientUtils::GetKmsDecryptRequest(
      1, encryption_key, kms_decrypt_request);
  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          SC_PRIVATE_KEY_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND)));
}

TEST(PrivateKeyClientUtilsTest,
     GetKmsDecryptRequestWithInvalidKeyResourceNameFailed) {
  auto encryption_key = CreateEncryptionKey("invalid");
  DecryptRequest kms_decrypt_request;
  auto result = PrivateKeyClientUtils::GetKmsDecryptRequest(
      1, encryption_key, kms_decrypt_request);
  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_KEY_RESOURCE_NAME)));
}

shared_ptr<EncryptionKey> CreateSinglePartyEncryptionKey(
    int8_t key_data_count = 1,
    const std::string& key_material = kSinglePartyKeyMaterialJson) {
  auto encryption_key = CreateEncryptionKeyBase();
  encryption_key->encryption_key_type =
      EncryptionKeyType::kSinglePartyHybridKey;
  for (int i = 0; i < key_data_count; ++i) {
    auto key_data = make_shared<KeyData>();
    key_data->key_encryption_key_uri =
        make_shared<std::string>(kTestKeyEncryptionKeyUriWithPrefix);
    key_data->key_material = make_shared<std::string>(key_material);
    key_data->public_key_signature =
        make_shared<std::string>(kTestPublicKeySignature);
    encryption_key->key_data.emplace_back(key_data);
  }
  return encryption_key;
}

TEST(PrivateKeyClientUtilsTest, GetKmsDecryptRequestForSinglePartySucceeded) {
  auto encryption_key = CreateSinglePartyEncryptionKey();
  DecryptRequest kms_decrypt_request;
  auto result = PrivateKeyClientUtils::GetKmsDecryptRequest(
      1, encryption_key, kms_decrypt_request);
  EXPECT_SUCCESS(result);
  // Fill the key with padding.
  std::string unescaped_key;
  absl::Base64Unescape("singlepartykey", &unescaped_key);
  std::string escaped_key;
  absl::Base64Escape(unescaped_key, &escaped_key);
  EXPECT_EQ(kms_decrypt_request.ciphertext(), escaped_key);
  EXPECT_EQ(kms_decrypt_request.key_resource_name(), kTestKeyEncryptionKeyUri);
}

TEST(PrivateKeyClientUtilsTest,
     GetKmsDecryptRequestForSinglePartyFailedForInvalidEndpointCount) {
  auto encryption_key = CreateSinglePartyEncryptionKey();
  DecryptRequest kms_decrypt_request;
  auto result = PrivateKeyClientUtils::GetKmsDecryptRequest(
      2, encryption_key, kms_decrypt_request);
  EXPECT_THAT(
      result,
      ResultIs(FailureExecutionResult(
          SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_VENDING_ENDPOINT_COUNT)));
}

TEST(PrivateKeyClientUtilsTest,
     GetKmsDecryptRequestForSinglePartyFailedForInvalidKeyDataCount) {
  auto encryption_key =
      CreateSinglePartyEncryptionKey(2, kSinglePartyKeyMaterialJson);
  DecryptRequest kms_decrypt_request;
  auto result = PrivateKeyClientUtils::GetKmsDecryptRequest(
      1, encryption_key, kms_decrypt_request);
  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_KEY_DATA_COUNT)));
}

TEST(PrivateKeyClientUtilsTest,
     GetKmsDecryptRequestForSinglePartyFailedForInvalidJsonKeyset) {
  auto encryption_key = CreateSinglePartyEncryptionKey(1, "invalidjson");
  DecryptRequest kms_decrypt_request;
  auto result = PrivateKeyClientUtils::GetKmsDecryptRequest(
      1, encryption_key, kms_decrypt_request);
  EXPECT_THAT(
      result,
      ResultIs(FailureExecutionResult(
          SC_PRIVATE_KEY_CLIENT_PROVIDER_CANNOT_READ_ENCRYPTED_KEY_SET)));
}

TEST(PrivateKeyClientUtilsTest, GetPrivateKeyInfo) {
  auto encryption_key = CreateEncryptionKey();

  PrivateKey private_key;
  auto result =
      PrivateKeyClientUtils::GetPrivateKeyInfo(encryption_key, private_key);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(private_key.key_id(), "name_test");
  EXPECT_EQ(private_key.public_key(), kTestPublicKeyMaterial);
  ExpectTimestampEquals(private_key.expiration_time(),
                        TimeUtil::MillisecondsToTimestamp(kTestExpirationTime));
  ExpectTimestampEquals(private_key.creation_time(),
                        TimeUtil::MillisecondsToTimestamp(kTestCreationTime));
}

TEST(PrivateKeyClientUtilsTest, ReconstructXorKeysetHandle) {
  std::string message = "Test message";
  std::vector<std::string> endpoint_responses = {
      "\270G\005\364$\253\273\331\353\336\216>",
      "\327\002\204 \232\377\002\330\225DB\f",
      "; \362\240\2369\334r\r\373\253W"};

  std::string private_key;
  auto result = PrivateKeyClientUtils::ReconstructXorKeysetHandle(
      endpoint_responses, private_key);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(private_key, message);
}

TEST(PrivateKeyClientUtilsTest,
     ReconstructXorKeysetHandleFailedWithInvalidInputs) {
  std::vector<std::string> endpoint_responses = {
      "\270G\005\364$\253\273\331\353\336\216>",
      "\327\002\204 \232\377\002\330", "; \362\240\2369\334r\r\373\253W"};

  std::string private_key;
  auto result = PrivateKeyClientUtils::ReconstructXorKeysetHandle(
      endpoint_responses, private_key);
  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_CLIENT_PROVIDER_SECRET_PIECE_SIZE_UNMATCHED)));
  EXPECT_TRUE(private_key.empty());
}

}  // namespace google::scp::cpio::client_providers::test
