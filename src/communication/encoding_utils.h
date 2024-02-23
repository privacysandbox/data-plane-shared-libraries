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

#ifndef COMMUNICATION_ENCODING_UTILS_H_
#define COMMUNICATION_ENCODING_UTILS_H_

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace privacy_sandbox::server_common {

// Number of bits to represent the payload version number in the first byte of
// an encoded payload.
inline constexpr int kNumFramingVersionBits = 3;
inline constexpr int kNumCompressionTypeBits = 8 - kNumFramingVersionBits;

// Sizes (in bytes) of fields in the encoded response payload.
inline constexpr int kFramingVersionAndCompressionTypeSizeBytes = 1;
inline constexpr int kCompressedDataSizeBytes = 4;

enum class CompressionType { kUncompressed = 0, kBrotli, kGzip = 2 };

struct DecodedRequest {
  int framing_version;
  CompressionType compression_type;
  std::string compressed_data;
};

// Encodes a response payload according to the following format:
// - 1 byte containing:
//   - 3 bits for the framing version (the format/structure of the payload)
//   - 5 bits for the compression algorithm used
// - 4 bytes for the size of compressed data
// - X bytes of compressed data
// - Y bytes of padding
// The output is a byte string (base64).
//
// The input of this method should be the compressed data + the algo used, and
// The output of this method should be used as the input for HPKE encryption.
absl::StatusOr<std::string> EncodeResponsePayload(
    CompressionType compression_type, std::string_view compressed_data,
    int encoded_data_size);

// Parses an encoded request payload and returns the compressed payload.
// See EncodeResponsePayload() for the expected encoded input. The input should
// be a byte string (Base64 encoded). Any issues reading the encoded payloads
// are returned as InvalidArgument errors because they're assumed to be from the
// payloads not having enough data to be read.
//
// The input to this method should be the output of HPKE decryption, and the
// output of this method should be the input to decompression.
absl::StatusOr<DecodedRequest> DecodeRequestPayload(std::string_view payload);

}  // namespace privacy_sandbox::server_common

#endif  // COMMUNICATION_ENCODING_UTILS_H_
