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

#include "src/communication/compression_gzip.h"

#include <zlib.h>

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_join.h"
#include "quiche/common/quiche_data_writer.h"

namespace privacy_sandbox::server_common {

namespace {

// Responsible for compressing one compression group (see compression.h for the
// compressed partition output format).
absl::StatusOr<std::string> CompressOnePartition(std::string_view partition) {
  z_stream zs = {
      .next_in = reinterpret_cast<Bytef*>(const_cast<char*>(partition.data())),
      .avail_in = static_cast<uInt>(partition.size()),
      .zalloc = Z_NULL,
      .zfree = Z_NULL,
      .opaque = Z_NULL,
  };
  if (const int deflate_init_status = deflateInit2(
          &zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, kGzipWindowBits | 16,
          kDefaultMemLevel, Z_DEFAULT_STRATEGY);
      deflate_init_status != Z_OK) {
    return absl::InternalError(
        absl::StrCat("Error initializing data for gzip compression (deflate "
                     "init status: ",
                     deflate_init_status, ")"));
  }

  // Determine the upper bound on the size of the compressed data.
  // Add sizeof(uint32_t) (e.g. 4 bytes) for the size of the compressed data.
  // TODO(b/279967613): Investigate if deflate() should be done in chunks like
  //  inflate(). Would help avoid this cpplint warning.
  const int kPartitionSizeBound =
      sizeof(uint32_t) + deflateBound(&zs, partition.size());
  char partition_output_buffer[kPartitionSizeBound];

  zs.avail_out = (uInt)kPartitionSizeBound;
  // For next_out, give the z_stream a reference to the output_buffer arr with
  // an offset of 4. We'll manually write the size of the compressed data
  // manually after compressing the data.
  zs.next_out = (Bytef*)&partition_output_buffer[sizeof(uint32_t)];

  if (const int deflate_status = deflate(&zs, Z_FINISH);
      deflate_status != Z_STREAM_END) {
    deflateEnd(&zs);
    return absl::InternalError(absl::StrCat(
        "Error compressing data using gzip (deflate status: ", deflate_status,
        ")"));
  }

  // Free all memory held by the z_stream object.
  if (const int deflate_end_status = deflateEnd(&zs);
      deflate_end_status != Z_OK) {
    return absl::InternalError(absl::StrCat(
        "Error closing compression data stream (deflate end status: ",
        deflate_end_status, ")"));
  }

  // Write the size of the compressed data into the first 4 indices of the
  // output_buffer.
  const size_t partition_final_size = sizeof(uint32_t) + zs.total_out;
  quiche::QuicheDataWriter data_writer(sizeof(uint32_t),
                                       partition_output_buffer);
  data_writer.WriteUInt32(zs.total_out);

  return std::string{partition_output_buffer, partition_final_size};
}

absl::StatusOr<std::string> DecompressString(
    std::string_view compressed_string) {
  z_stream zs = {
      .next_in =
          reinterpret_cast<Bytef*>(const_cast<char*>(compressed_string.data())),
      .avail_in = static_cast<uInt>(compressed_string.size()),
  };
  if (const int inflate_init_status = inflateInit2(&zs, kGzipWindowBits | 16);
      inflate_init_status != Z_OK) {
    return absl::InternalError(
        absl::StrCat("Error during gzip decompression initialization: (inflate "
                     "init status: ",
                     inflate_init_status, ")"));
  }

  char output_buffer[32768];  // 32 KiB chunks.
  std::string decompressed_string;

  int inflate_status;
  do {
    zs.next_out = reinterpret_cast<Bytef*>(output_buffer);
    zs.avail_out = sizeof(output_buffer);

    inflate_status = inflate(&zs, Z_NO_FLUSH);
    // Copy the decompressed output from the buffer to our result string.
    if (decompressed_string.size() < zs.total_out) {
      decompressed_string.append(output_buffer,
                                 zs.total_out - decompressed_string.size());
    }
  } while (inflate_status == Z_OK);
  if (inflate_status != Z_STREAM_END) {
    inflateEnd(&zs);
    return absl::DataLossError(
        absl::StrCat("Exception during gzip decompression: (inflate status: ",
                     inflate_status, ")"));
  }

  if (const int inflate_end_status = inflateEnd(&zs);
      inflate_end_status != Z_OK) {
    return absl::InternalError(absl::StrCat(
        "Error closing compression data stream (inflate end status: ",
        inflate_end_status, ")"));
  }
  return decompressed_string;
}

}  // namespace

absl::StatusOr<std::string> GzipCompressionGroupConcatenator::Build() const {
  std::vector<std::string> compression_groups;
  for (const auto& partition : Partitions()) {
    if (auto maybe_partition_output = CompressOnePartition(partition);
        !maybe_partition_output.ok()) {
      return maybe_partition_output.status();
    } else {
      compression_groups.push_back(std::move(maybe_partition_output).value());
    }
  }

  return absl::StrJoin(compression_groups, "");
}

absl::StatusOr<std::string>
GzipCompressionBlobReader::ExtractOneCompressionGroup() {
  uint32_t compression_group_size = 0;
  if (!data_reader_.ReadUInt32(&compression_group_size)) {
    return absl::InvalidArgumentError("Failed to read compression group size.");
  }
  VLOG(9) << "compression_group_size: " << compression_group_size;

  std::string_view compressed_data;
  if (!data_reader_.ReadStringPiece(&compressed_data, compression_group_size)) {
    return absl::InvalidArgumentError("Failed to read compression group.");
  }

  return DecompressString(compressed_data);
}

}  // namespace privacy_sandbox::server_common
