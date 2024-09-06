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

#include "src/cpio/client_providers/private_key_client_provider/private_key_client_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/util/time_util.h>

#include "absl/strings/escaping.h"
#include "src/core/interface/http_types.h"
#include "src/core/test/utils/timestamp_test_utils.h"
#include "src/core/utils/base64.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

namespace google::scp::cpio::client_providers::test {
namespace {

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::private_key_service::v1::PrivateKey;
using google::protobuf::util::TimeUtil;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
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
using google::scp::core::utils::Base64Encode;
using google::scp::cpio::client_providers::KeyData;
using google::scp::cpio::client_providers::PrivateKeyFetchingResponse;
using ::testing::StrEq;

constexpr std::string_view kTestKeyId = "name_test";
constexpr std::string_view kTestResourceName = "encryptionKeys/name_test";
constexpr std::string_view kTestPublicKeysetHandle = "publicKeysetHandle";
constexpr std::string_view kTestPublicKeyMaterial = "publicKeyMaterial";
constexpr int kTestExpirationTime = 123456;
constexpr int kTestCreationTime = 111111;
constexpr std::string_view kTestPublicKeySignature = "publicKeySignature";
constexpr std::string_view kTestKeyEncryptionKeyUriWithPrefix =
    "1234567890keyEncryptionKeyUri";
constexpr std::string_view kTestKeyEncryptionKeyUri = "keyEncryptionKeyUri";
constexpr std::string_view kTestKeyMaterial = "keyMaterial";
constexpr std::string_view kSinglePartyKeyMaterialJson =
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

std::shared_ptr<EncryptionKey> CreateEncryptionKeyBase() {
  auto encryption_key = std::make_shared<EncryptionKey>();
  encryption_key->key_id = std::make_shared<std::string>(kTestKeyId);
  encryption_key->resource_name =
      std::make_shared<std::string>(kTestResourceName);
  encryption_key->expiration_time_in_ms = kTestExpirationTime;
  encryption_key->creation_time_in_ms = kTestCreationTime;
  encryption_key->public_key_material =
      std::make_shared<std::string>(kTestPublicKeyMaterial);
  encryption_key->public_keyset_handle =
      std::make_shared<std::string>(kTestPublicKeysetHandle);
  return encryption_key;
}

std::shared_ptr<EncryptionKey> CreateEncryptionKey(
    std::string_view key_resource_name = kTestKeyEncryptionKeyUriWithPrefix) {
  auto encryption_key = CreateEncryptionKeyBase();
  encryption_key->encryption_key_type =
      EncryptionKeyType::kMultiPartyHybridEvenKeysplit;
  auto key_data = std::make_shared<KeyData>();
  key_data->key_encryption_key_uri =
      std::make_shared<std::string>(key_resource_name);
  key_data->key_material = std::make_shared<std::string>(kTestKeyMaterial);
  key_data->public_key_signature =
      std::make_shared<std::string>(kTestPublicKeySignature);
  encryption_key->key_data.emplace_back(key_data);
  return encryption_key;
}

TEST(PrivateKeyClientUtilsTest, GetKmsDecryptRequestSuccess) {
  auto encryption_key = CreateEncryptionKey();
  DecryptRequest kms_decrypt_request;
  ASSERT_SUCCESS(PrivateKeyClientUtils::GetKmsDecryptRequest(
      encryption_key, kms_decrypt_request));
  EXPECT_THAT(kms_decrypt_request.ciphertext(), StrEq(kTestKeyMaterial));
  EXPECT_THAT(kms_decrypt_request.key_resource_name(),
              StrEq(kTestKeyEncryptionKeyUri));
}

TEST(PrivateKeyClientUtilsTest, GetKmsDecryptRequestFailed) {
  auto encryption_key = CreateEncryptionKey();

  auto key_data = std::make_shared<KeyData>();
  key_data->key_encryption_key_uri = std::make_shared<std::string>("");
  key_data->key_material = std::make_shared<std::string>("");
  key_data->public_key_signature = std::make_shared<std::string>("");
  encryption_key->key_data = std::vector<std::shared_ptr<KeyData>>({key_data});

  DecryptRequest kms_decrypt_request;
  auto result = PrivateKeyClientUtils::GetKmsDecryptRequest(
      encryption_key, kms_decrypt_request);
  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          SC_PRIVATE_KEY_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND)));
}

TEST(PrivateKeyClientUtilsTest,
     GetKmsDecryptRequestWithInvalidKeyResourceNameFailed) {
  auto encryption_key = CreateEncryptionKey("invalid");
  DecryptRequest kms_decrypt_request;
  auto result = PrivateKeyClientUtils::GetKmsDecryptRequest(
      encryption_key, kms_decrypt_request);
  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_KEY_RESOURCE_NAME)));
}

std::shared_ptr<EncryptionKey> CreateSinglePartyEncryptionKey(
    int8_t key_data_count = 1,
    std::string_view key_material = kSinglePartyKeyMaterialJson) {
  auto encryption_key = CreateEncryptionKeyBase();
  encryption_key->encryption_key_type =
      EncryptionKeyType::kSinglePartyHybridKey;
  for (int i = 0; i < key_data_count; ++i) {
    auto key_data = std::make_shared<KeyData>();
    key_data->key_encryption_key_uri =
        std::make_shared<std::string>(kTestKeyEncryptionKeyUriWithPrefix);
    key_data->key_material = std::make_shared<std::string>(key_material);
    key_data->public_key_signature =
        std::make_shared<std::string>(kTestPublicKeySignature);
    encryption_key->key_data.emplace_back(key_data);
  }
  return encryption_key;
}

TEST(PrivateKeyClientUtilsTest, GetKmsDecryptRequestForSinglePartySucceeded) {
  auto encryption_key = CreateSinglePartyEncryptionKey();
  DecryptRequest kms_decrypt_request;
  ASSERT_SUCCESS(PrivateKeyClientUtils::GetKmsDecryptRequest(
      encryption_key, kms_decrypt_request));
  // Fill the key with padding.
  std::string unescaped_key;
  absl::Base64Unescape("singlepartykey", &unescaped_key);
  std::string escaped_key;
  absl::Base64Escape(unescaped_key, &escaped_key);
  EXPECT_THAT(kms_decrypt_request.ciphertext(), StrEq(escaped_key));
  EXPECT_THAT(kms_decrypt_request.key_resource_name(),
              StrEq(kTestKeyEncryptionKeyUri));
}

TEST(PrivateKeyClientUtilsTest,
     GetKmsDecryptRequestForSinglePartyFailedForInvalidKeyDataCount) {
  auto encryption_key =
      CreateSinglePartyEncryptionKey(2, kSinglePartyKeyMaterialJson);
  DecryptRequest kms_decrypt_request;
  auto result = PrivateKeyClientUtils::GetKmsDecryptRequest(
      encryption_key, kms_decrypt_request);
  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_KEY_DATA_COUNT)));
}

TEST(PrivateKeyClientUtilsTest,
     GetKmsDecryptRequestForSinglePartyFailedForInvalidJsonKeyset) {
  auto encryption_key = CreateSinglePartyEncryptionKey(1, "invalidjson");
  DecryptRequest kms_decrypt_request;
  auto result = PrivateKeyClientUtils::GetKmsDecryptRequest(
      encryption_key, kms_decrypt_request);
  EXPECT_THAT(
      result,
      ResultIs(FailureExecutionResult(
          SC_PRIVATE_KEY_CLIENT_PROVIDER_CANNOT_READ_ENCRYPTED_KEY_SET)));
}

DecryptResult CreateDecryptResult(
    std::string plaintext,
    ExecutionResult decrypt_result = SuccessExecutionResult(),
    bool multi_party_key = true) {
  auto encryption_key = CreateEncryptionKey();
  if (!multi_party_key) {
    encryption_key = CreateSinglePartyEncryptionKey();
  }
  return DecryptResult{
      .decrypt_result = decrypt_result,
      .encryption_key = std::move(*encryption_key),
      .plaintext = std::move(plaintext),
  };
}

TEST(PrivateKeyClientUtilsTest, ConsturctPrivateKeySuccess) {
  std::vector<DecryptResult> decrypt_results;
  decrypt_results.emplace_back(
      CreateDecryptResult("\270G\005\364$\253\273\331\353\336\216>"));
  decrypt_results.emplace_back(
      CreateDecryptResult("\327\002\204 \232\377\002\330\225DB\f"));
  decrypt_results.emplace_back(
      CreateDecryptResult("; \362\240\2369\334r\r\373\253W"));

  auto private_key_or =
      PrivateKeyClientUtils::ConstructPrivateKey(decrypt_results);
  ASSERT_SUCCESS(private_key_or);
  auto private_key = *private_key_or;
  EXPECT_EQ(private_key.key_id(), "name_test");
  EXPECT_EQ(private_key.public_key(), kTestPublicKeyMaterial);
  ExpectTimestampEquals(private_key.expiration_time(),
                        TimeUtil::MillisecondsToTimestamp(kTestExpirationTime));
  ExpectTimestampEquals(private_key.creation_time(),
                        TimeUtil::MillisecondsToTimestamp(kTestCreationTime));
  std::string encoded_key;
  Base64Encode("Test message", encoded_key);
  EXPECT_EQ(private_key.private_key(), encoded_key);
}

TEST(PrivateKeyClientUtilsTest,
     ConsturctPrivateKeyFailedWithUnmatchedPlaintextSize) {
  std::vector<DecryptResult> decrypt_results;
  decrypt_results.emplace_back(
      CreateDecryptResult("\270G\005\364$\253\273\331\353\336\216>"));
  decrypt_results.emplace_back(
      CreateDecryptResult("\327\002\204 \232\377\002\330"));
  decrypt_results.emplace_back(
      CreateDecryptResult("; \362\240\2369\334r\r\373\253W"));

  auto private_key_or =
      PrivateKeyClientUtils::ConstructPrivateKey(decrypt_results);
  EXPECT_THAT(private_key_or,
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_CLIENT_PROVIDER_SECRET_PIECE_SIZE_UNMATCHED)));
}

TEST(PrivateKeyClientUtilsTest,
     ConsturctPrivateKeyFailedWithEmptyDecryptResult) {
  std::vector<DecryptResult> decrypt_results;

  auto private_key_or =
      PrivateKeyClientUtils::ConstructPrivateKey(decrypt_results);
  EXPECT_THAT(private_key_or,
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND)));
}

TEST(PrivateKeyClientUtilsTest, ExtractAnyFailureNoFailure) {
  std::vector<KeysResultPerEndpoint> keys_result_list(2);
  {
    absl::MutexLock lock(&keys_result_list[0].mu);
    keys_result_list[0].fetch_result_key_id_map.insert(
        {"key1", SuccessExecutionResult()});
    keys_result_list[0].fetch_result_key_id_map.insert(
        {"key2", SuccessExecutionResult()});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("")});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("")});
  }

  {
    absl::MutexLock lock(&keys_result_list[1].mu);
    keys_result_list[1].fetch_result_key_id_map.insert(
        {"key1", SuccessExecutionResult()});
    keys_result_list[1].fetch_result_key_id_map.insert(
        {"key2", SuccessExecutionResult()});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("")});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("")});
  }

  EXPECT_SUCCESS(
      PrivateKeyClientUtils::ExtractAnyFailure(keys_result_list, "key1"));
  EXPECT_SUCCESS(
      PrivateKeyClientUtils::ExtractAnyFailure(keys_result_list, "key2"));
}

TEST(PrivateKeyClientUtilsTest, ExtractAnyFailureReturnFetchFailure) {
  std::vector<KeysResultPerEndpoint> keys_result_list(2);
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  {
    absl::MutexLock lock(&keys_result_list[0].mu);
    keys_result_list[0].fetch_result_key_id_map.insert(
        {"key1", SuccessExecutionResult()});
    keys_result_list[0].fetch_result_key_id_map.insert(
        {"key2", SuccessExecutionResult()});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("")});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("")});
    keys_result_list[0].fetch_result = failure;
  }

  {
    absl::MutexLock lock(&keys_result_list[1].mu);
    keys_result_list[1].fetch_result_key_id_map.insert(
        {"key1", SuccessExecutionResult()});
    keys_result_list[1].fetch_result_key_id_map.insert(
        {"key2", SuccessExecutionResult()});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("")});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("")});
  }

  EXPECT_THAT(
      PrivateKeyClientUtils::ExtractAnyFailure(keys_result_list, "key1"),
      ResultIs(failure));
  EXPECT_THAT(
      PrivateKeyClientUtils::ExtractAnyFailure(keys_result_list, "key2"),
      ResultIs(failure));
}

TEST(PrivateKeyClientUtilsTest, ExtractAnyFailureReturnFetchFailureForOneKey) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  std::vector<KeysResultPerEndpoint> keys_result_list(2);
  {
    absl::MutexLock lock(&keys_result_list[0].mu);
    keys_result_list[0].fetch_result_key_id_map.insert({"key1", failure});
    keys_result_list[0].fetch_result_key_id_map.insert(
        {"key2", SuccessExecutionResult()});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("")});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("")});
  }

  {
    absl::MutexLock lock(&keys_result_list[1].mu);
    keys_result_list[1].fetch_result_key_id_map.insert(
        {"key1", SuccessExecutionResult()});
    keys_result_list[1].fetch_result_key_id_map.insert(
        {"key2", SuccessExecutionResult()});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("")});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("")});
  }

  EXPECT_THAT(
      PrivateKeyClientUtils::ExtractAnyFailure(keys_result_list, "key1"),
      ResultIs(failure));
  EXPECT_SUCCESS(
      PrivateKeyClientUtils::ExtractAnyFailure(keys_result_list, "key2"));
}

TEST(PrivateKeyClientUtilsTest,
     ExtractAnyFailureReturnFetchFailureForBothKeys) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  std::vector<KeysResultPerEndpoint> keys_result_list(2);
  {
    absl::MutexLock lock(&keys_result_list[0].mu);
    keys_result_list[0].fetch_result_key_id_map.insert({"key1", failure});
    keys_result_list[0].fetch_result_key_id_map.insert(
        {"key2", SuccessExecutionResult()});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("")});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("")});
  }

  {
    absl::MutexLock lock(&keys_result_list[1].mu);
    keys_result_list[1].fetch_result_key_id_map.insert(
        {"key1", SuccessExecutionResult()});
    keys_result_list[1].fetch_result_key_id_map.insert({"key2", failure});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("")});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("")});
  }

  EXPECT_THAT(
      PrivateKeyClientUtils::ExtractAnyFailure(keys_result_list, "key1"),
      ResultIs(failure));
  EXPECT_THAT(
      PrivateKeyClientUtils::ExtractAnyFailure(keys_result_list, "key2"),
      ResultIs(failure));
}

TEST(PrivateKeyClientUtilsTest,
     ExtractAnyFailureReturnDecryptFailureForOneKey) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  std::vector<KeysResultPerEndpoint> keys_result_list(2);
  {
    absl::MutexLock lock(&keys_result_list[0].mu);
    keys_result_list[0].fetch_result_key_id_map.insert(
        {"key1", SuccessExecutionResult()});
    keys_result_list[0].fetch_result_key_id_map.insert(
        {"key2", SuccessExecutionResult()});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("", failure)});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("")});
  }

  {
    absl::MutexLock lock(&keys_result_list[1].mu);
    keys_result_list[1].fetch_result_key_id_map.insert(
        {"key1", SuccessExecutionResult()});
    keys_result_list[1].fetch_result_key_id_map.insert(
        {"key2", SuccessExecutionResult()});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("")});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("")});
  }

  EXPECT_THAT(
      PrivateKeyClientUtils::ExtractAnyFailure(keys_result_list, "key1"),
      ResultIs(failure));
  EXPECT_SUCCESS(
      PrivateKeyClientUtils::ExtractAnyFailure(keys_result_list, "key2"));
}

TEST(PrivateKeyClientUtilsTest,
     ExtractAnyFailureReturnDecryptFailureForBothKeys) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  std::vector<KeysResultPerEndpoint> keys_result_list(2);
  {
    absl::MutexLock lock(&keys_result_list[0].mu);
    keys_result_list[0].fetch_result_key_id_map.insert(
        {"key1", SuccessExecutionResult()});
    keys_result_list[0].fetch_result_key_id_map.insert(
        {"key2", SuccessExecutionResult()});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("", failure)});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("")});
  }

  {
    absl::MutexLock lock(&keys_result_list[1].mu);
    keys_result_list[1].fetch_result_key_id_map.insert(
        {"key1", SuccessExecutionResult()});
    keys_result_list[1].fetch_result_key_id_map.insert(
        {"key2", SuccessExecutionResult()});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("")});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("", failure)});
  }

  EXPECT_THAT(
      PrivateKeyClientUtils::ExtractAnyFailure(keys_result_list, "key1"),
      ResultIs(failure));
  EXPECT_THAT(
      PrivateKeyClientUtils::ExtractAnyFailure(keys_result_list, "key2"),
      ResultIs(failure));
}

TEST(PrivateKeyClientUtilsTest, ExtractAnyFailureFetchResultNotFound) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  std::vector<KeysResultPerEndpoint> keys_result_list(2);
  {
    absl::MutexLock lock(&keys_result_list[0].mu);
    keys_result_list[0].fetch_result_key_id_map.insert({"key1", failure});
    keys_result_list[0].fetch_result_key_id_map.insert({"key2", failure});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("", failure)});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key3", CreateDecryptResult("")});
  }

  {
    absl::MutexLock lock(&keys_result_list[1].mu);
    keys_result_list[1].fetch_result_key_id_map.insert({"key1", failure});
    keys_result_list[1].fetch_result_key_id_map.insert({"key2", failure});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("")});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key3", CreateDecryptResult("")});
  }

  EXPECT_SUCCESS(
      PrivateKeyClientUtils::ExtractAnyFailure(keys_result_list, "key3"));
}

TEST(PrivateKeyClientUtilsTest, ExtractAnyFailureDecryptResultNotFound) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  std::vector<KeysResultPerEndpoint> keys_result_list(2);
  {
    absl::MutexLock lock(&keys_result_list[0].mu);
    keys_result_list[0].fetch_result_key_id_map.insert(
        {"key1", SuccessExecutionResult()});
    keys_result_list[0].fetch_result_key_id_map.insert(
        {"key3", SuccessExecutionResult()});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("", failure)});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("", failure)});
  }

  {
    absl::MutexLock lock(&keys_result_list[1].mu);
    keys_result_list[1].fetch_result_key_id_map.insert(
        {"key1", SuccessExecutionResult()});
    keys_result_list[1].fetch_result_key_id_map.insert(
        {"key3", SuccessExecutionResult()});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("", failure)});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("", failure)});
  }

  EXPECT_SUCCESS(
      PrivateKeyClientUtils::ExtractAnyFailure(keys_result_list, "key3"));
}

TEST(PrivateKeyClientUtilsTest, ExtractSinglePartyKeyReturnNoKey) {
  std::vector<KeysResultPerEndpoint> keys_result_list(2);
  {
    absl::MutexLock lock(&keys_result_list[0].mu);
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("")});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("")});
  }

  {
    absl::MutexLock lock(&keys_result_list[1].mu);
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("")});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("")});
  }

  EXPECT_FALSE(
      PrivateKeyClientUtils::ExtractSinglePartyKey(keys_result_list, "key1")
          .has_value());
  EXPECT_FALSE(
      PrivateKeyClientUtils::ExtractSinglePartyKey(keys_result_list, "key2")
          .has_value());
  EXPECT_FALSE(
      PrivateKeyClientUtils::ExtractSinglePartyKey(keys_result_list, "key3")
          .has_value());
}

TEST(PrivateKeyClientUtilsTest, ExtractSinglePartyKeyReturnKey) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  std::vector<KeysResultPerEndpoint> keys_result_list(2);
  {
    absl::MutexLock lock(&keys_result_list[0].mu);
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("", failure, false)});
    keys_result_list[0].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("", failure, false)});
  }

  {
    absl::MutexLock lock(&keys_result_list[1].mu);
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key1", CreateDecryptResult("", failure)});
    keys_result_list[1].decrypt_result_key_id_map.insert(
        {"key2", CreateDecryptResult("", failure, false)});
  }

  EXPECT_TRUE(
      PrivateKeyClientUtils::ExtractSinglePartyKey(keys_result_list, "key1")
          .has_value());
  EXPECT_TRUE(
      PrivateKeyClientUtils::ExtractSinglePartyKey(keys_result_list, "key2")
          .has_value());
  EXPECT_FALSE(
      PrivateKeyClientUtils::ExtractSinglePartyKey(keys_result_list, "key3")
          .has_value());
}
}  // namespace
}  // namespace google::scp::cpio::client_providers::test
