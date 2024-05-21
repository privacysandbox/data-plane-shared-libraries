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

#include "src/communication/ohttp_utils.h"

#include <limits>
#include <memory>
#include <string>

#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "quiche/oblivious_http/buffers/oblivious_http_request.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"
#include "quiche/oblivious_http/oblivious_http_gateway.h"
#include "src/encryption/key_fetcher/interface/private_key_fetcher_interface.h"
#include "src/util/status_macro/status_macros.h"

namespace privacy_sandbox::server_common {
namespace {

// Parameters used to configure Oblivious HTTP.
// KEM: DHKEM(X25519, HKDF-SHA256) 0x0020
constexpr uint16_t kX25519HkdfSha256KemId = EVP_HPKE_DHKEM_X25519_HKDF_SHA256;
// KDF: HKDF-SHA256 0x0001
constexpr uint16_t kHkdfSha256Id = EVP_HPKE_HKDF_SHA256;
// AEAD: AES-256-GCM 0x0002
constexpr uint16_t kAes256GcmAeadId = EVP_HPKE_AES_256_GCM;

// TODO(b/269787188): Remove once KMS starts returning numeric key IDs.
absl::StatusOr<uint8_t> ToIntKeyId(std::string_view key_id) {
  uint32_t val;
  if (!absl::SimpleAtoi(key_id, &val) ||
      val > std::numeric_limits<uint8_t>::max()) {
    return absl::InternalError(
        absl::StrCat("Cannot parse OHTTP key ID from: ", key_id));
  }

  return val;
}

// Examines the encapsulated request and determines which request label (AKA
// media type) should be used to decrypt the request.
absl::StatusOr<std::string_view> GetRequestLabel(
    std::string_view encapsulated_request) {
  static_assert((kOHTTPHeaderValue & kOHTTPHeaderCompareMask) ==
                kOHTTPHeaderValue);

  quiche::QuicheDataReader reader(encapsulated_request);
  uint64_t read_number64;
  if (!reader.ReadUInt64(&read_number64)) {
    VLOG(2) << "Unable to read first 64 bits of encapsulated request";
    return absl::InvalidArgumentError(kInvalidEncapsulatedRequestFormat);
  }

  // First, check to see if this encapsulated request follows the *old* format.
  if ((read_number64 & kOHTTPHeaderCompareMask) == kOHTTPHeaderValue) {
    return quiche::ObliviousHttpHeaderKeyConfig::kOhttpRequestLabel;
  }

  // Next, check to see if this encapsulated request follows the *new* format
  // where the first byte is a zero followed by the encapsulated request.
  if ((read_number64 & 0xFF00000000000000) != 0) {
    // In the future, we can branch here depending on the value of the first
    // byte in the request. But today, we reject the request if it's not a zero.
    VLOG(2) << "Detected request format using B&A request label, but expected 0"
               "for the first byte of the encapsulated request";
    return absl::InvalidArgumentError(kInvalidEncapsulatedRequestFormat);
  }

  uint64_t read_number64_shifted = read_number64 << 8;
  if ((read_number64_shifted & kOHTTPHeaderCompareMask) != kOHTTPHeaderValue) {
    VLOG(2) << "Unable to match encapsulated request with either expected"
               "request format";
    return absl::InvalidArgumentError(kInvalidEncapsulatedRequestFormat);
  }

  return kBiddingAuctionOhttpRequestLabel;
}

}  // namespace

absl::StatusOr<EncapsulatedRequest> ParseEncapsulatedRequest(
    std::string_view encapsulated_request) {
  PS_ASSIGN_OR_RETURN(const auto request_label,
                      GetRequestLabel(encapsulated_request));

  // If the request label is B&A's custom label, the first byte is a zero and
  // the actual OHTTP request begins from the second byte. So trim off the first
  // byte.
  if (request_label == kBiddingAuctionOhttpRequestLabel) {
    encapsulated_request = encapsulated_request.substr(1);
  }

  return EncapsulatedRequest{encapsulated_request, request_label};
}

absl::StatusOr<quiche::ObliviousHttpRequest> DecryptEncapsulatedRequest(
    const PrivateKey& private_key, const EncapsulatedRequest& request) {
  const auto key_id = ToIntKeyId(private_key.key_id);
  if (!key_id.ok()) {
    return absl::InternalError(key_id.status().message());
  }

  const auto key_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      key_id.value(), kX25519HkdfSha256KemId, kHkdfSha256Id, kAes256GcmAeadId);
  if (!key_config.ok()) {
    return absl::InternalError(
        absl::StrCat("Failed to build OHTTP header config: ",
                     key_config.status().message()));
  }

  // Validate the encapsulated request matches the expected key config above.
  const auto payload_headers =
      key_config->ParseOhttpPayloadHeader(request.request_payload);
  if (!payload_headers.ok()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Unsupported HPKE primitive ID provided in encapsulated request: ",
        payload_headers.message()));
  }

  const absl::StatusOr<quiche::ObliviousHttpGateway> gateway =
      quiche::ObliviousHttpGateway::Create(private_key.private_key,
                                           *key_config);
  if (!gateway.ok()) {
    return absl::InternalError(gateway.status().message());
  }

  absl::StatusOr<quiche::ObliviousHttpRequest> decrypted_req =
      gateway->DecryptObliviousHttpRequest(request.request_payload,
                                           request.request_label);
  if (!decrypted_req.ok()) {
    if (absl::IsInvalidArgument(decrypted_req.status())) {
      // Likely a malformed payload.
      return decrypted_req;
    }

    return absl::InternalError(absl::StrCat("Unable to decrypt ciphertext: ",
                                            decrypted_req.status().message()));
  }

  return decrypted_req;
}

absl::StatusOr<std::string> EncryptAndEncapsulateResponse(
    const std::string plaintext_data, const PrivateKey& private_key,
    quiche::ObliviousHttpRequest::Context& context,
    std::string_view request_label) {
  const auto key_id = ToIntKeyId(private_key.key_id);
  if (!key_id.ok()) {
    return absl::InternalError(key_id.status().message());
  }

  const auto config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      key_id.value(), kX25519HkdfSha256KemId, kHkdfSha256Id, kAes256GcmAeadId);
  if (!config.ok()) {
    return absl::InternalError(absl::StrCat(
        "Failed to build OHTTP header config: ", config.status().message()));
  }

  const auto gateway =
      quiche::ObliviousHttpGateway::Create(private_key.private_key, *config);
  if (!gateway.ok()) {
    return absl::InternalError(gateway.status().message());
  }

  // Based off of the request label, use the corresponding response label for
  // response encryption.
  std::string_view response_label =
      (request_label == kBiddingAuctionOhttpRequestLabel)
          ? kBiddingAuctionOhttpResponseLabel
          : quiche::ObliviousHttpHeaderKeyConfig::kOhttpResponseLabel;

  const auto oblivious_response = gateway->CreateObliviousHttpResponse(
      std::move(plaintext_data), context, response_label);
  if (!oblivious_response.ok()) {
    return absl::InternalError(
        absl::StrCat("Failed to create OHTTP response: ",
                     oblivious_response.status().message()));
  }

  return oblivious_response.value().EncapsulateAndSerialize();
}

}  // namespace privacy_sandbox::server_common
