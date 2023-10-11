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
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/util/time_util.h>
#include <tink/json_keyset_reader.h>

#include "absl/strings/escaping.h"
#include "core/interface/http_types.h"
#include "cpio/client_providers/interface/private_key_fetcher_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

#include "error_codes.h"

using crypto::tink::JsonKeysetReader;
using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::private_key_service::v1::PrivateKey;
using google::protobuf::util::TimeUtil;
using google::scp::core::ExecutionResult;
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
using std::byte;
using std::shared_ptr;

namespace {
constexpr char kPrivateKeyClientUtils[] = "PrivateKeyClientUtils";
// The keyUri returned from KeyVendingService contains prefix "gcp-kms://" or
// "aws-kms://", and we need to remove it before sending for decryption.
constexpr int kKeyArnPrefixSize = 10;
}  // namespace

namespace google::scp::cpio::client_providers {
ExecutionResult PrivateKeyClientUtils::GetKmsDecryptRequest(
    const size_t endpoint_count,
    const shared_ptr<EncryptionKey>& encryption_key,
    DecryptRequest& kms_decrypt_request) noexcept {
  if (encryption_key->encryption_key_type ==
      EncryptionKeyType::kSinglePartyHybridKey) {
    if (endpoint_count != 1) {
      auto execution_result = FailureExecutionResult(
          SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_VENDING_ENDPOINT_COUNT);
      SCP_ERROR(kPrivateKeyClientUtils, kZeroUuid, execution_result,
                "Expect the endpoint count to be 1, but is %lld.",
                endpoint_count);
      return execution_result;
    }
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

ExecutionResult PrivateKeyClientUtils::GetPrivateKeyInfo(
    const shared_ptr<EncryptionKey>& encryption_key,
    PrivateKey& private_key) noexcept {
  private_key.set_key_id(*encryption_key->key_id);
  private_key.set_public_key(*encryption_key->public_key_material);
  *private_key.mutable_expiration_time() =
      TimeUtil::MillisecondsToTimestamp(encryption_key->expiration_time_in_ms);
  *private_key.mutable_creation_time() =
      TimeUtil::MillisecondsToTimestamp(encryption_key->creation_time_in_ms);

  return SuccessExecutionResult();
}

/**
 * @brief Convert string to a vector of byte.
 *
 * @param string
 * @return std::vector<byte> vector of byte.
 */
static std::vector<byte> StrToBytes(const std::string& string) noexcept {
  std::vector<byte> bytes;
  for (char c : string) {
    bytes.push_back(byte(c));
  }
  return bytes;
}

/**
 * @brief XOR operation for two vectors of byte.
 *
 * @param arr1 vector of byte.
 * @param arr2 vector of byte.
 * @return std::vector<byte> Exclusive OR result of the two input vectors.
 */
static std::vector<byte> XOR(const std::vector<byte>& arr1,
                             const std::vector<byte>& arr2) noexcept {
  std::vector<byte> result;
  for (int i = 0; i < arr1.size(); ++i) {
    result.push_back((byte)(arr1[i] ^ arr2[i]));
  }

  return result;
}

ExecutionResult PrivateKeyClientUtils::ReconstructXorKeysetHandle(
    const std::vector<std::string>& endpoint_responses,
    std::string& private_key) noexcept {
  std::vector<byte> xor_secret = StrToBytes(endpoint_responses.at(0));

  for (auto i = 1; i < endpoint_responses.size(); ++i) {
    std::vector<byte> next_piece = StrToBytes(endpoint_responses.at(i));

    if (xor_secret.size() != next_piece.size()) {
      return FailureExecutionResult(
          SC_PRIVATE_KEY_CLIENT_PROVIDER_SECRET_PIECE_SIZE_UNMATCHED);
    }

    xor_secret = XOR(xor_secret, next_piece);
  }
  std::string key_string(reinterpret_cast<const char*>(&xor_secret[0]),
                         xor_secret.size());
  private_key = key_string;
  return SuccessExecutionResult();
}

}  // namespace google::scp::cpio::client_providers
