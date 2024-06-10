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

#ifndef CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_CLIENT_PROVIDER_PRIVATE_KEY_CLIENT_UTILS_H_
#define CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_CLIENT_PROVIDER_PRIVATE_KEY_CLIENT_UTILS_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "src/core/interface/http_types.h"
#include "src/cpio/client_providers/interface/kms_client_provider_interface.h"
#include "src/cpio/client_providers/interface/private_key_fetcher_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
/// Which kind of filter to apply when listing private keys.
enum class ListingMethod {
  kByKeyId = 1,
  kByMaxAge = 2,
};

/// Objects for decrypt result.
struct DecryptResult {
  core::ExecutionResult decrypt_result;
  EncryptionKey encryption_key;
  std::string plaintext;
};

/// Fetch and decrypt results for one endpoint.
struct KeysResultPerEndpoint {
  // If the ListingMethod is kByKeyId, each key ID will have a ExecutionResult
  // which will be stored in the map here.
  absl::flat_hash_map<std::string, core::ExecutionResult>
      fetch_result_key_id_map ABSL_GUARDED_BY(mu);
  // If the ListingMethod is kByMaxAge, all keys for each endpoint are sharing
  // the same ExecutionResult which will be stored here.
  core::ExecutionResult fetch_result = core::SuccessExecutionResult();
  absl::flat_hash_map<std::string, DecryptResult> decrypt_result_key_id_map
      ABSL_GUARDED_BY(mu);
  absl::Mutex mu;
};

class PrivateKeyClientUtils {
 public:
  /**
   * @brief Get the Kms Decrypt Request object.
   *
   * @param encryption_key EncryptionKey object.
   * @param[out] kms_decrypt_request KmsDecryptRequest object.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult GetKmsDecryptRequest(
      const std::shared_ptr<EncryptionKey>& encryption_key,
      cmrt::sdk::kms_service::v1::DecryptRequest& kms_decrypt_request) noexcept;
  /**
   * @brief Convert the given string to a vector of byte.
   *
   * @param str the given string.
   * @return vector<byte> vector of byte.
   */
  static std::vector<std::byte> StrToBytes(std::string_view str) noexcept;

  /**
   * @brief XOR operation for two vectors of byte.
   *
   * @param arr1 vector of byte.
   * @param arr2 vector of byte.
   * @return std::vector<byte> Exclusive OR result of the two input vectors.
   */
  static std::vector<std::byte> XOR(
      const std::vector<std::byte>& arr1,
      const std::vector<std::byte>& arr2) noexcept;

  /**
   * @brief Construct private key from decrypt result.
   *
   * @param decrypt_results decrypt results.
   * @return core::ExecutionResultOr<PrivateKey> construct result.
   */
  static core::ExecutionResultOr<cmrt::sdk::private_key_service::v1::PrivateKey>
  ConstructPrivateKey(
      const std::vector<DecryptResult>& decrypt_results) noexcept;

  /**
   * @brief Extract fetch or decrypt failure result from key results for the
   * given key ID.
   *
   * @param keys_result all key results.
   * @param key_id the given key ID.
   * @return core::ExecutionResult failure result.
   */
  static core::ExecutionResult ExtractAnyFailure(
      std::vector<KeysResultPerEndpoint>& keys_result,
      const std::string& key_id) noexcept;

  /**
   * @brief Extract single party key result for the given key ID.
   *
   * @param keys_result all key results.
   * @param key_id the given key ID.
   * @param endpoint_count endpoint count.
   * @return std::Optional<DecryptResult> single party key result.
   */
  static std::optional<DecryptResult> ExtractSinglePartyKey(
      std::vector<KeysResultPerEndpoint>& keys_result,
      const std::string& key_id) noexcept;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_CLIENT_PROVIDER_PRIVATE_KEY_CLIENT_UTILS_H_
