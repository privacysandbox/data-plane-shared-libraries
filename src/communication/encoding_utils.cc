// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/communication/encoding_utils.h"

#include <math.h>

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/common/quiche_data_reader.h"
#include "quiche/common/quiche_data_writer.h"

namespace privacy_sandbox::server_common {

absl::StatusOr<std::string> EncodeResponsePayload(
    CompressionType compression_type, std::string_view compressed_data,
    int encoded_data_size) {
  int min_required_payload_size = kFramingVersionAndCompressionTypeSizeBytes +
                                  kCompressedDataSizeBytes +
                                  compressed_data.size();
  if (encoded_data_size < min_required_payload_size) {
    const std::string error = absl::StrCat(
        "Payload too large to be encoded to the given size: "
        "(encoded_data_size: ",
        encoded_data_size,
        ", minimum size required: ", min_required_payload_size, ")");
    return absl::InternalError(error);
  }

  // Re-define encoded_data_size as a const to get a cpplint warning to go away.
  const int kEncodedDataSize = encoded_data_size;
  char buffer[kEncodedDataSize];
  quiche::QuicheDataWriter writer(sizeof(buffer), buffer);

  // 1. Write the framing version  and compression algorithm in one byte.
  // framing_version is zero for now but can change in the future.
  const int framing_version = 0;
  const uint8_t first_byte =
      (framing_version << 5) | static_cast<int>(compression_type);
  writer.WriteUInt8(first_byte);

  // 2. Write the length of the compressed data *and* the compressed data.
  writer.WriteUInt32(compressed_data.size());
  writer.WriteStringPiece(compressed_data);

  // 3. Fill the rest of the buffer with padding (e.g. zeroes).
  writer.WritePadding();

  return std::string(buffer, sizeof(buffer));
}

absl::StatusOr<DecodedRequest> DecodeRequestPayload(std::string_view payload) {
  quiche::QuicheDataReader reader(payload);

  uint8_t first_byte;
  if (!reader.ReadUInt8(&first_byte)) {
    return absl::InvalidArgumentError(
        "Failed to parse framing version and compression algorithm.");
  }

  const int version_num = first_byte >> kNumCompressionTypeBits;
  const int compression_type =
      first_byte & ((int)pow(2, kNumCompressionTypeBits) - 1);

  uint32_t compressed_data_length;
  if (!reader.ReadUInt32(&compressed_data_length)) {
    return absl::InvalidArgumentError(
        "Failed to parse compressed data length.");
  }

  std::string_view compressed_data;
  if (!reader.ReadStringPiece(&compressed_data, compressed_data_length)) {
    return absl::InvalidArgumentError("Failed to parse compressed data.");
  }

  // The rest of the payload is padding and can be ignored.

  DecodedRequest req;
  req.framing_version = version_num;
  req.compression_type = static_cast<CompressionType>(compression_type);
  req.compressed_data = std::string(compressed_data);
  return req;
}

}  // namespace privacy_sandbox::server_common
