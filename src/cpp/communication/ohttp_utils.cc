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

#include "src/cpp/communication/ohttp_utils.h"

#include <limits>
#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "quiche/oblivious_http/buffers/oblivious_http_request.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"
#include "quiche/oblivious_http/oblivious_http_gateway.h"
#include "src/cpp/encryption/key_fetcher/interface/private_key_fetcher_interface.h"

namespace privacy_sandbox::server_common {
namespace {

// Parameters used to configure Oblivious HTTP.
// KEM: DHKEM(X25519, HKDF-SHA256) 0x0020
inline constexpr uint16_t kX25519HkdfSha256KemId =
    EVP_HPKE_DHKEM_X25519_HKDF_SHA256;
// KDF: HKDF-SHA256 0x0001
inline constexpr uint16_t kHkdfSha256Id = EVP_HPKE_HKDF_SHA256;
// AEAD: AES-256-GCM 0x0002
inline constexpr uint16_t kAes256GcmAeadId = EVP_HPKE_AES_256_GCM;

// TODO(b/269787188): Remove once KMS starts returning numeric key IDs.
absl::StatusOr<uint8_t> ToIntKeyId(absl::string_view key_id) {
  uint32_t val;
  if (!absl::SimpleAtoi(key_id, &val) ||
      val > std::numeric_limits<uint8_t>::max()) {
    return absl::InternalError(
        absl::StrCat("Cannot parse OHTTP key ID from: ", key_id));
  }

  return val;
}

}  // namespace

absl::StatusOr<uint8_t> ParseKeyId(
    absl::string_view ohttp_encapsulated_request) {
  return quiche::ObliviousHttpHeaderKeyConfig::
      ParseKeyIdFromObliviousHttpRequestPayload(ohttp_encapsulated_request);
}

absl::StatusOr<quiche::ObliviousHttpRequest> DecryptEncapsulatedRequest(
    const PrivateKey& private_key, absl::string_view encapsulated_request) {
  const auto key_id = ToIntKeyId(private_key.key_id);
  if (!key_id.ok()) {
    return absl::Status(absl::StatusCode::kInternal, key_id.status().message());
  }

  const auto key_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      key_id.value(), kX25519HkdfSha256KemId, kHkdfSha256Id, kAes256GcmAeadId);
  if (!key_config.ok()) {
    const std::string error = absl::StrCat(
        "Failed to build OHTTP header config: ", key_config.status().message());
    return absl::Status(absl::StatusCode::kInternal, error);
  }

  // Validate the encapsulated request matches the expected key config above.
  const auto payload_headers =
      key_config->ParseOhttpPayloadHeader(encapsulated_request);
  if (!payload_headers.ok()) {
    const std::string error = absl::StrCat(
        "Unsupported HPKE primitive ID provided in encapsulated request: ",
        payload_headers.message());
    return absl::Status(absl::StatusCode::kInvalidArgument, error);
  }

  const absl::StatusOr<quiche::ObliviousHttpGateway> gateway =
      quiche::ObliviousHttpGateway::Create(private_key.private_key,
                                           *key_config);
  if (!gateway.ok()) {
    return absl::Status(absl::StatusCode::kInternal,
                        gateway.status().message());
  }

  absl::StatusOr<quiche::ObliviousHttpRequest> decrypted_req =
      gateway->DecryptObliviousHttpRequest(encapsulated_request);
  if (!decrypted_req.ok()) {
    if (absl::IsInvalidArgument(decrypted_req.status())) {
      // Likely a malformed payload.
      return decrypted_req;
    }

    const std::string error = absl::StrCat("Unable to decrypt ciphertext: ",
                                           decrypted_req.status().message());
    return absl::Status(absl::StatusCode::kInternal, error);
  }

  return decrypted_req;
}

absl::StatusOr<std::string> EncryptAndEncapsulateResponse(
    const std::string plaintext_data, const PrivateKey& private_key,
    quiche::ObliviousHttpRequest::Context& context) {
  const auto key_id = ToIntKeyId(private_key.key_id);
  if (!key_id.ok()) {
    return absl::Status(absl::StatusCode::kInternal, key_id.status().message());
  }

  const auto config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      key_id.value(), kX25519HkdfSha256KemId, kHkdfSha256Id, kAes256GcmAeadId);
  if (!config.ok()) {
    const std::string error = absl::StrCat(
        "Failed to build OHTTP header config: ", config.status().message());
    return absl::Status(absl::StatusCode::kInternal, error);
  }

  const auto gateway =
      quiche::ObliviousHttpGateway::Create(private_key.private_key, *config);
  if (!gateway.ok()) {
    return absl::Status(absl::StatusCode::kInternal,
                        gateway.status().message());
  }

  const auto oblivious_response =
      gateway->CreateObliviousHttpResponse(std::move(plaintext_data), context);
  if (!oblivious_response.ok()) {
    const std::string error =
        absl::StrCat("Failed to create OHTTP response: ",
                     oblivious_response.status().message());
    return absl::Status(absl::StatusCode::kInternal, error);
  }

  return oblivious_response.value().EncapsulateAndSerialize();
}

}  // namespace privacy_sandbox::server_common
