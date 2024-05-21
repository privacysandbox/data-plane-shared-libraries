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

#ifndef COMMUNICATION_COMPRESSION_GZIP_H_
#define COMMUNICATION_COMPRESSION_GZIP_H_

#include <string>

#include "src/communication/compression.h"

namespace privacy_sandbox::server_common {

// As per the documentation in zlib.h, "Add 16 to windowBits to write a simple
// gzip header and trailer around the compressed data instead of a zlib
// wrapper." 15 is the default value, so add/OR 16 to it.
inline constexpr int kGzipWindowBits = 15 | 16;
// Default to 8 as per zlib.h's documentation.
inline constexpr int kDefaultMemLevel = 8;

// Builds compression groups that are compressed by gzip.
class GzipCompressionGroupConcatenator : public CompressionGroupConcatenator {
 public:
  absl::StatusOr<std::string> Build() const override;
};

// Reads compression groups built with GzipCompressionGroupConcatenator.
class GzipCompressionBlobReader : public CompressedBlobReader {
 public:
  explicit GzipCompressionBlobReader(std::string_view compressed)
      : CompressedBlobReader(compressed) {}

  absl::StatusOr<std::string> ExtractOneCompressionGroup() override;
};

}  // namespace privacy_sandbox::server_common

#endif  // COMMUNICATION_COMPRESSION_GZIP_H_
