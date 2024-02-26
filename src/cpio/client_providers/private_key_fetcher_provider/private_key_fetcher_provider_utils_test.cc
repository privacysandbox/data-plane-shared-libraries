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

#include "src/cpio/client_providers/private_key_fetcher_provider/private_key_fetcher_provider_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "src/core/interface/http_types.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

namespace google::scp::cpio::client_providers::test {
namespace {

using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::
    SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_RESOURCE_NAME;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_CREATION_TIME_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_ENCRYPTION_KEY_TYPE_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_EXPIRATION_TIME_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_INVALID_ENCRYPTION_KEY_TYPE;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_KEY_DATA_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_KEY_MATERIAL_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_PUBLIC_KEYSET_HANDLE_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHER_PROVIDER_RESOURCE_NAME_NOT_FOUND;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::IsSuccessfulAndHolds;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::KeyData;
using google::scp::cpio::client_providers::PrivateKeyFetchingResponse;
using ::testing::IsEmpty;
using ::testing::StrEq;

constexpr std::string_view kKeyId = "123";
constexpr std::string_view kPrivateKeyBaseUri = "http://localhost.test:8000";

TEST(PrivateKeyFetchingClientUtilsTest, ParsePrivateKeySuccess) {
  std::string bytes_str = R"({
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
    })";

  PrivateKeyFetchingResponse response;
  ASSERT_SUCCESS(PrivateKeyFetchingClientUtils::ParsePrivateKey(
      BytesBuffer(bytes_str), response));
  EXPECT_EQ(response.encryption_keys.size(), 1);
  const auto& encryption_key = *response.encryption_keys.begin();
  EXPECT_THAT(*encryption_key->key_id, StrEq("123456"));
  EXPECT_THAT(*encryption_key->resource_name, StrEq("encryptionKeys/123456"));
  EXPECT_EQ(encryption_key->encryption_key_type,
            EncryptionKeyType::kMultiPartyHybridEvenKeysplit);
  EXPECT_THAT(*encryption_key->public_keyset_handle, StrEq("primaryKeyId"));
  EXPECT_THAT(*encryption_key->public_key_material, StrEq("testtest"));
  EXPECT_EQ(encryption_key->expiration_time_in_ms, 1669943990485);
  EXPECT_EQ(encryption_key->creation_time_in_ms, 1669252790485);
  EXPECT_THAT(*encryption_key->key_data[0]->key_encryption_key_uri,
              StrEq("aws-kms://arn:aws:kms:us-east-1:1234567:key"));
  EXPECT_THAT(*encryption_key->key_data[0]->public_key_signature, IsEmpty());
  EXPECT_THAT(*encryption_key->key_data[0]->key_material, StrEq("test=test"));
  EXPECT_THAT(*encryption_key->key_data[1]->key_encryption_key_uri,
              StrEq("aws-kms://arn:aws:kms:us-east-1:12345:key"));
  EXPECT_THAT(*encryption_key->key_data[1]->public_key_signature, IsEmpty());
  EXPECT_THAT(*encryption_key->key_data[1]->key_material, IsEmpty());
}

TEST(PrivateKeyFetchingClientUtilsTest, FailedWithInvalidKeyData) {
  std::string bytes_str = R"({
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
                "keyEncryptionKeyUri": "",
                "keyMaterial": "test=test"
            },
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:12345:key",
                "keyMaterial": ""
            }
        ]
    })";

  PrivateKeyFetchingResponse response;
  auto result = PrivateKeyFetchingClientUtils::ParsePrivateKey(
      BytesBuffer(bytes_str), response);

  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_FETCHER_PROVIDER_KEY_MATERIAL_NOT_FOUND)));
}

TEST(PrivateKeyFetchingClientUtilsTest, FailedWithInvalidKeyDataNoKeyUri) {
  std::string bytes_str = R"({
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
                "keyMaterial": ""
            },
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:12345:key",
                "keyMaterial": ""
            }
        ]
    })";

  PrivateKeyFetchingResponse response;
  auto result = PrivateKeyFetchingClientUtils::ParsePrivateKey(
      BytesBuffer(bytes_str), response);

  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_FETCHER_PROVIDER_KEY_MATERIAL_NOT_FOUND)));
}

TEST(PrivateKeyFetchingClientUtilsTest, FailedWithInvalidKeyType) {
  std::string bytes_str = R"({
        "name": "encryptionKeys/123456",
        "encryptionKeyType": "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT_WRONG",
        "publicKeysetHandle": "primaryKeyId",
        "publicKeyMaterial": "testtest",
        "creationTime": "1669252790485",
        "expirationTime": "1669943990485",
        "ttlTime": 0,
        "keyData": [
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:1234567:key",
                "keyMaterial": ""
            },
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:12345:key",
                "keyMaterial": ""
            }
        ]
    })";

  PrivateKeyFetchingResponse response;
  auto result = PrivateKeyFetchingClientUtils::ParsePrivateKey(
      BytesBuffer(bytes_str), response);

  auto failure_result = FailureExecutionResult(
      SC_PRIVATE_KEY_FETCHER_PROVIDER_INVALID_ENCRYPTION_KEY_TYPE);
  EXPECT_THAT(result, ResultIs(failure_result));
}

TEST(PrivateKeyFetchingClientUtilsTest, FailedWithNameNotFound) {
  std::string bytes_str = R"({
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
                "keyMaterial": ""
            },
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:12345:key",
                "keyMaterial": ""
            }
        ]
    })";

  PrivateKeyFetchingResponse response;
  auto result = PrivateKeyFetchingClientUtils::ParsePrivateKey(
      BytesBuffer(bytes_str), response);

  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_FETCHER_PROVIDER_RESOURCE_NAME_NOT_FOUND)));
}

TEST(PrivateKeyFetchingClientUtilsTest, FailedWithExpirationTimeNotFound) {
  std::string bytes_str = R"({
        "name": "encryptionKeys/123456",
        "encryptionKeyType": "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT",
        "publicKeysetHandle": "primaryKeyId",
        "publicKeyMaterial": "testtest",
        "creationTime": "1669252790485",
        "ttlTime": 0,
        "keyData": [
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:1234567:key",
                "keyMaterial": ""
            },
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:12345:key",
                "keyMaterial": ""
            }
        ]
    })";

  PrivateKeyFetchingResponse response;
  auto result = PrivateKeyFetchingClientUtils::ParsePrivateKey(
      BytesBuffer(bytes_str), response);

  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_FETCHER_PROVIDER_EXPIRATION_TIME_NOT_FOUND)));
}

TEST(PrivateKeyFetchingClientUtilsTest, FailedWithCreationTimeNotFound) {
  std::string bytes_str = R"({
        "name": "encryptionKeys/123456",
        "encryptionKeyType": "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT",
        "publicKeysetHandle": "primaryKeyId",
        "publicKeyMaterial": "testtest",
        "expirationTime": "1669943990485",
        "ttlTime": 0,
        "keyData": [
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:1234567:key",
                "keyMaterial": ""
            },
            {
                "publicKeySignature": "",
                "keyEncryptionKeyUri": "aws-kms://arn:aws:kms:us-east-1:12345:key",
                "keyMaterial": ""
            }
        ]
    })";

  PrivateKeyFetchingResponse response;
  auto result = PrivateKeyFetchingClientUtils::ParsePrivateKey(
      BytesBuffer(bytes_str), response);

  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_FETCHER_PROVIDER_CREATION_TIME_NOT_FOUND)));
}

TEST(PrivateKeyFetchingClientUtilsTest, CreateHttpRequestForKeyId) {
  PrivateKeyFetchingRequest request;
  request.key_vending_endpoint = std::make_shared<PrivateKeyVendingEndpoint>();
  request.key_vending_endpoint->private_key_vending_service_endpoint =
      kPrivateKeyBaseUri;
  request.key_id = std::make_shared<std::string>(kKeyId);
  request.max_age_seconds = 1000000;
  HttpRequest http_request;
  PrivateKeyFetchingClientUtils::CreateHttpRequest(request, http_request);

  EXPECT_EQ(http_request.method, HttpMethod::GET);
  EXPECT_THAT(*http_request.path, StrEq(std::string(kPrivateKeyBaseUri) + "/" +
                                        std::string(kKeyId)));
}

TEST(PrivateKeyFetchingClientUtilsTest, CreateHttpRequestForMaxAgeSeconds) {
  PrivateKeyFetchingRequest request;
  request.key_vending_endpoint = std::make_shared<PrivateKeyVendingEndpoint>();
  request.key_vending_endpoint->private_key_vending_service_endpoint =
      kPrivateKeyBaseUri;
  request.max_age_seconds = 1000000;
  HttpRequest http_request;
  PrivateKeyFetchingClientUtils::CreateHttpRequest(request, http_request);

  EXPECT_EQ(http_request.method, HttpMethod::GET);
  EXPECT_THAT(*http_request.path,
              StrEq(std::string(kPrivateKeyBaseUri) + ":recent"));
  EXPECT_THAT(*http_request.query, StrEq("maxAgeSeconds=1000000"));
}

TEST(PrivateKeyFetchingClientUtilsTest, ParseMultiplePrivateKeysSuccess) {
  std::string one_key_without_name = R"(
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
    })";
  std::string key_1 = R"({"name": "encryptionKeys/111111",)";
  std::string key_2 = R"({"name": "encryptionKeys/222222",)";
  std::string bytes_str = R"({"keys": [)" + key_1 + one_key_without_name + "," +
                          key_2 + one_key_without_name + "]}";

  PrivateKeyFetchingResponse response;
  ASSERT_SUCCESS(PrivateKeyFetchingClientUtils::ParsePrivateKey(
      BytesBuffer(bytes_str), response));
  EXPECT_EQ(response.encryption_keys.size(), 2);
  EXPECT_THAT(*response.encryption_keys[0]->key_id, StrEq("111111"));
  EXPECT_THAT(*response.encryption_keys[1]->key_id, StrEq("222222"));
}

TEST(PrivateKeyFetchingClientUtilsTest, ExtractKeyId) {
  auto key_id_or =
      PrivateKeyFetchingClientUtils::ExtractKeyId("encryptionKeys/123456");
  EXPECT_THAT(key_id_or, IsSuccessfulAndHolds(StrEq("123456")));

  key_id_or = PrivateKeyFetchingClientUtils::ExtractKeyId("encryption/123456");
  EXPECT_THAT(key_id_or,
              ResultIs(FailureExecutionResult(
                  SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_RESOURCE_NAME)));
}
}  // namespace
}  // namespace google::scp::cpio::client_providers::test
