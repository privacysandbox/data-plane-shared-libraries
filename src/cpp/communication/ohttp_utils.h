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

#ifndef SRC_CPP_COMMUNICATION_OHTTP_UTILS_H_
#define SRC_CPP_COMMUNICATION_OHTTP_UTILS_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "glog/logging.h"
#include "quiche/oblivious_http/buffers/oblivious_http_request.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"
#include "quiche/oblivious_http/oblivious_http_gateway.h"
#include "src/cpp/encryption/key_fetcher/interface/private_key_fetcher_interface.h"

// ohttp_utils.h contains functions for converting an encapsulated OHTTP request
// to a proto and the reverse. If any individual step in the decryption/
// deserialization process fails, the methods appropriately return
// InternalError or InvalidArgument statuses, depending on whether the error
// is caused by malformed input or a server side error.
namespace privacy_sandbox::server_common {

// Reads the key ID from the first 8 bits of an encapsulated OHTTP request.
// Returns InvalidArgument if key ID cannot be parsed.
absl::StatusOr<uint8_t> ParseKeyId(
    absl::string_view ohttp_encapsulated_request);

// Decrypts an OHTTP encapsulated request assuming the request follows the
// DHKEM_X25519_HKDF_SHA256_HKDF_SHA256_AES_256_GCM format.
// Use ObliviousHttpRequest.GetPlaintextData() to get the decrypted request.
absl::StatusOr<quiche::ObliviousHttpRequest> DecryptEncapsulatedRequest(
    const PrivateKey& private_key, absl::string_view encapsulated_request);

// Encrypts and encapsulates data in OHTTP format. The OHTTP context returned
// from DecryptEncapsulatedRequest() is a required input to create the response.
// Clients should use std::move() to pass in the plaintext data.
absl::StatusOr<std::string> EncryptAndEncapsulateResponse(
    std::string plaintext_data, const PrivateKey& private_key,
    quiche::ObliviousHttpRequest::Context& context);

}  // namespace privacy_sandbox::server_common

#endif  // SRC_CPP_COMMUNICATION_OHTTP_UTILS_H_
