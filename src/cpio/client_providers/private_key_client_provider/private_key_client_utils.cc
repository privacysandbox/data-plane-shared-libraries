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

#include "private_key_client_utils.h"

#include <bitset>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/util/time_util.h>
#include <tink/json_keyset_reader.h>

#include "absl/strings/escaping.h"
#include "src/core/interface/http_types.h"
#include "src/core/utils/base64.h"
#include "src/cpio/client_providers/interface/private_key_fetcher_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

#include "error_codes.h"

using crypto::tink::JsonKeysetReader;
using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::private_key_service::v1::PrivateKey;
using google::protobuf::util::TimeUtil;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpHeaders;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_PRIVATE_KEY_CLIENT_PROVIDER_CANNOT_CREATE_JSON_KEY_SET;
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
using google::scp::cpio::client_providers::KeyData;
using google::scp::cpio::client_providers::PrivateKeyFetchingResponse;

namespace {
constexpr std::string_view kPrivateKeyClientUtils = "PrivateKeyClientUtils";
// The keyUri returned from KeyVendingService contains prefix "gcp-kms://" or
// "aws-kms://", and we need to remove it before sending for decryption.
constexpr int kKeyArnPrefixSize = 10;
}  // namespace

namespace google::scp::cpio::client_providers {
ExecutionResult PrivateKeyClientUtils::GetKmsDecryptRequest(
    const std::shared_ptr<EncryptionKey>& encryption_key,
    DecryptRequest& kms_decrypt_request) noexcept {
  if (encryption_key->encryption_key_type ==
      EncryptionKeyType::kSinglePartyHybridKey) {
    if (encryption_key->key_data.size() != 1) {
      auto execution_result = FailureExecutionResult(
          SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_KEY_DATA_COUNT);
      SCP_ERROR(kPrivateKeyClientUtils, kZeroUuid, execution_result,
                "Expect the key data count to be 1, but is %lld.",
                encryption_key->key_data.size());
      return execution_result;
    }
    auto key_data = *encryption_key->key_data.begin();
    auto keyset_reader_or = JsonKeysetReader::New(*key_data->key_material);
    if (!keyset_reader_or.ok()) {
      auto execution_result = FailureExecutionResult(
          SC_PRIVATE_KEY_CLIENT_PROVIDER_CANNOT_CREATE_JSON_KEY_SET);
      SCP_ERROR(kPrivateKeyClientUtils, kZeroUuid, execution_result,
                "Failed to create JsonKeysetReader: %s.",
                keyset_reader_or.status().ToString().c_str());
      return execution_result;
    }
    auto keyset_or = (*keyset_reader_or)->ReadEncrypted();
    if (!keyset_or.ok()) {
      auto execution_result = FailureExecutionResult(
          SC_PRIVATE_KEY_CLIENT_PROVIDER_CANNOT_READ_ENCRYPTED_KEY_SET);
      SCP_ERROR(kPrivateKeyClientUtils, kZeroUuid, execution_result,
                "Failed to parse encryption key set: %s.",
                keyset_or.status().ToString().c_str());
      return execution_result;
    }
    // JsonKeysetReader unescapes the key material, so we escape it back.
    std::string escaped_ciphertext;
    absl::Base64Escape((*keyset_or)->encrypted_keyset(), &escaped_ciphertext);
    kms_decrypt_request.set_ciphertext(std::move(escaped_ciphertext));
    kms_decrypt_request.set_key_resource_name(
        key_data->key_encryption_key_uri->substr(kKeyArnPrefixSize));
    return SuccessExecutionResult();
  } else if (encryption_key->encryption_key_type ==
             EncryptionKeyType::kMultiPartyHybridEvenKeysplit) {
    for (auto key_data : encryption_key->key_data) {
      if (key_data->key_material && !key_data->key_material->empty()) {
        if (key_data->key_encryption_key_uri->size() < kKeyArnPrefixSize) {
          return FailureExecutionResult(
              SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_KEY_RESOURCE_NAME);
        }
        kms_decrypt_request.set_key_resource_name(
            key_data->key_encryption_key_uri->substr(kKeyArnPrefixSize));
        kms_decrypt_request.set_ciphertext(*key_data->key_material);
        return SuccessExecutionResult();
      }
    }
  }

  return FailureExecutionResult(
      SC_PRIVATE_KEY_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND);
}

std::vector<std::byte> PrivateKeyClientUtils::StrToBytes(
    std::string_view string) noexcept {
  std::vector<std::byte> bytes;
  for (char c : string) {
    bytes.push_back(std::byte(c));
  }
  return bytes;
}

std::vector<std::byte> PrivateKeyClientUtils::XOR(
    const std::vector<std::byte>& arr1,
    const std::vector<std::byte>& arr2) noexcept {
  std::vector<std::byte> result;
  for (int i = 0; i < arr1.size(); ++i) {
    result.push_back((std::byte)(arr1[i] ^ arr2[i]));
  }

  return result;
}

ExecutionResultOr<PrivateKey> PrivateKeyClientUtils::ConstructPrivateKey(
    const std::vector<DecryptResult>& decrypt_results) noexcept {
  if (decrypt_results.empty()) {
    return FailureExecutionResult(
        SC_PRIVATE_KEY_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND);
  }

  auto& encryption_key = decrypt_results.at(0).encryption_key;
  PrivateKey private_key;
  private_key.set_key_id(*encryption_key.key_id);
  private_key.set_public_key(*encryption_key.public_key_material);
  *private_key.mutable_expiration_time() =
      TimeUtil::MillisecondsToTimestamp(encryption_key.expiration_time_in_ms);
  *private_key.mutable_creation_time() =
      TimeUtil::MillisecondsToTimestamp(encryption_key.creation_time_in_ms);

  std::vector<std::byte> xor_secret =
      StrToBytes(decrypt_results.at(0).plaintext);

  for (auto i = 1; i < decrypt_results.size(); ++i) {
    std::vector<std::byte> next_piece =
        StrToBytes(decrypt_results.at(i).plaintext);
    if (xor_secret.size() != next_piece.size()) {
      return FailureExecutionResult(
          SC_PRIVATE_KEY_CLIENT_PROVIDER_SECRET_PIECE_SIZE_UNMATCHED);
    }
    xor_secret = XOR(xor_secret, next_piece);
  }
  std::string key_string(reinterpret_cast<const char*>(&xor_secret[0]),
                         xor_secret.size());

  std::string encoded_key;
  RETURN_IF_FAILURE(core::utils::Base64Encode(key_string, encoded_key));
  private_key.set_private_key(encoded_key);
  return private_key;
}

ExecutionResult PrivateKeyClientUtils::ExtractAnyFailure(
    std::vector<KeysResultPerEndpoint>& keys_result,
    const std::string& key_id) noexcept {
  for (auto& key_result : keys_result) {
    RETURN_IF_FAILURE(key_result.fetch_result);

    absl::MutexLock lock(&key_result.mu);
    if (const auto it = key_result.fetch_result_key_id_map.find(key_id);
        it != key_result.fetch_result_key_id_map.end()) {
      RETURN_IF_FAILURE(it->second);
    }

    if (const auto it = key_result.decrypt_result_key_id_map.find(key_id);
        it != key_result.decrypt_result_key_id_map.end()) {
      RETURN_IF_FAILURE(it->second.decrypt_result);
    }
  }
  return SuccessExecutionResult();
}

std::optional<DecryptResult> PrivateKeyClientUtils::ExtractSinglePartyKey(
    std::vector<KeysResultPerEndpoint>& keys_result,
    const std::string& key_id) noexcept {
  for (auto& key_result : keys_result) {
    DecryptResult decrypt_result;
    absl::MutexLock lock(&key_result.mu);
    if (const auto it = key_result.decrypt_result_key_id_map.find(key_id);
        it != key_result.decrypt_result_key_id_map.end() &&
        it->second.encryption_key.encryption_key_type ==
            EncryptionKeyType::kSinglePartyHybridKey) {
      return it->second;
    }
  }
  return std::nullopt;
}

}  // namespace google::scp::cpio::client_providers
