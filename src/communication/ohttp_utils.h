/*
 * Copyright 2023 Google LLC
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

#ifndef COMMUNICATION_OHTTP_UTILS_H_
#define COMMUNICATION_OHTTP_UTILS_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "quiche/oblivious_http/buffers/oblivious_http_request.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"
#include "quiche/oblivious_http/oblivious_http_gateway.h"
#include "src/encryption/key_fetcher/interface/private_key_fetcher_interface.h"

// ohttp_utils.h contains functions for converting an encapsulated OHTTP request
// to a proto and the reverse. If any individual step in the decryption/
// deserialization process fails, the methods appropriately return
// InternalError or InvalidArgument statuses, depending on whether the error
// is caused by malformed input or a server side error.
namespace privacy_sandbox::server_common {

inline constexpr std::string_view kInvalidEncapsulatedRequestFormat =
    "Unrecognized encapsulated request format";

// The concatenated form of the expected HPKE primitives/config.
inline constexpr uint64_t kOHTTPHeaderValue =
    ((uint64_t)EVP_HPKE_DHKEM_X25519_HKDF_SHA256 << 40) |
    (EVP_HPKE_HKDF_SHA256 << 24) | (EVP_HPKE_AES_256_GCM << 8);
inline constexpr uint64_t kOHTTPHeaderCompareMask = 0x00FFFFFFFFFFFF00;

// Custom media types for B&A. Used as input to request decryption/response
// encryption.
inline constexpr std::string_view kBiddingAuctionOhttpRequestLabel =
    "message/auction request";
inline constexpr std::string_view kBiddingAuctionOhttpResponseLabel =
    "message/auction response";

struct EncapsulatedRequest {
  // The actual request to be decrypted.
  std::string_view request_payload;
  // The request_label (media type) that should be used to decrypt the request.
  std::string_view request_label;
};

// Examines the encapsulated request to a) find the starting point of the actual
// request and b) determine where the media type that should be used to decrypt
// the encapsulated request. The request payload returned from this method is
// the input to DecryptEncapsulatedRequest() below.
// There are two expected request formats:
// 1. 0x ??     - 0020 - 0001 - 0002
//       Key ID   KEM    KDF    AEAD
// Indicating that the "message/bhttp request" media type was used to generate
// the encapsulated request, or
// 2. 0x 00        - ??     - 0020 - 0001 - 0002
//       Zero byte   Key ID   KEM    KDF    AEAD
// the same format as (1) but prepended with a zero byte, indicating the B&A
// media type was used instead.
absl::StatusOr<EncapsulatedRequest> ParseEncapsulatedRequest(
    std::string_view encapsulated_request);

// Decrypts an OHTTP encapsulated request assuming the request follows the
// DHKEM_X25519_HKDF_SHA256_HKDF_SHA256_AES_256_GCM format.
// Use ObliviousHttpRequest.GetPlaintextData() to get the decrypted request.
absl::StatusOr<quiche::ObliviousHttpRequest> DecryptEncapsulatedRequest(
    const PrivateKey& private_key, const EncapsulatedRequest& request);

// Encrypts and encapsulates data in OHTTP format. The OHTTP context returned
// from DecryptEncapsulatedRequest() is a required input to create the response.
// Clients should use std::move() to pass in the plaintext data.
// The request_label should be passed from the result of
// DecryptEncapsulatedRequest().
absl::StatusOr<std::string> EncryptAndEncapsulateResponse(
    std::string plaintext_data, const PrivateKey& private_key,
    quiche::ObliviousHttpRequest::Context& context,
    std::string_view request_label);

}  // namespace privacy_sandbox::server_common

#endif  // COMMUNICATION_OHTTP_UTILS_H_
