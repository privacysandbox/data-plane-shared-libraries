// Copyright 2022 Google LLC
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

#include "src/communication/compression_brotli.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "absl/log/log.h"
#include "src/communication/uncompressed.h"

using ::testing::StrEq;

namespace privacy_sandbox::server_common {
namespace {

const std::string_view kTestString = "large message";
const std::string_view kTestString2 = "large message 2";

TEST(CompressionBlobReaderTest, Success) {
  const char brotli_data[14] = {static_cast<char>(0x83),
                                static_cast<char>(0x04),
                                static_cast<char>(0x80),
                                'q',
                                'w',
                                'e',
                                'r',
                                't',
                                'y',
                                'u',
                                'i',
                                'o',
                                'p',
                                static_cast<char>(0x03)};

  UncompressedConcatenator concatenator;

  concatenator.AddCompressionGroup(std::string(brotli_data, 14));
  auto maybe_compression_group_blob = concatenator.Build();
  ASSERT_TRUE(maybe_compression_group_blob.ok())
      << maybe_compression_group_blob.status();

  BrotliCompressionBlobReader blob_reader(*maybe_compression_group_blob);

  EXPECT_FALSE(blob_reader.IsDoneReading());

  auto maybe_compression_group = blob_reader.ExtractOneCompressionGroup();
  ASSERT_TRUE(maybe_compression_group.ok()) << maybe_compression_group.status();
  EXPECT_THAT(*maybe_compression_group, StrEq("qwertyuiop"));
  EXPECT_TRUE(blob_reader.IsDoneReading());
}

TEST(CompressionGroupConcatenatorTest, Success) {
  BrotliCompressionGroupConcatenator concatenator;
  concatenator.AddCompressionGroup(std::string(kTestString));
  concatenator.AddCompressionGroup(std::string(kTestString2));
  std::string large_message(500, 'a');
  concatenator.AddCompressionGroup(large_message);

  auto maybe_output = concatenator.Build();
  ASSERT_TRUE(maybe_output.ok()) << maybe_output.status();
  LOG(INFO) << "compressed size: " << maybe_output->size();

  BrotliCompressionBlobReader blob_reader(*maybe_output);

  EXPECT_FALSE(blob_reader.IsDoneReading());

  auto maybe_compression_group = blob_reader.ExtractOneCompressionGroup();
  ASSERT_TRUE(maybe_compression_group.ok()) << maybe_compression_group.status();
  EXPECT_THAT(*maybe_compression_group, StrEq(kTestString));
  EXPECT_FALSE(blob_reader.IsDoneReading());

  maybe_compression_group = blob_reader.ExtractOneCompressionGroup();
  ASSERT_TRUE(maybe_compression_group.ok()) << maybe_compression_group.status();
  EXPECT_THAT(*maybe_compression_group, StrEq(kTestString2));
  EXPECT_FALSE(blob_reader.IsDoneReading());

  maybe_compression_group = blob_reader.ExtractOneCompressionGroup();
  ASSERT_TRUE(maybe_compression_group.ok()) << maybe_compression_group.status();
  EXPECT_THAT(*maybe_compression_group, StrEq(large_message));
  EXPECT_TRUE(blob_reader.IsDoneReading());
}

}  // namespace
}  // namespace privacy_sandbox::server_common
