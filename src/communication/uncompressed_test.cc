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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string_view>

#include "src/communication/compression.h"

using ::testing::StrEq;

namespace privacy_sandbox::server_common {
namespace {

const std::string_view kTestString = "large message";
const std::string_view kTestString2 = "large message 2";

TEST(CompressionGroupConcatenatorTest, Success) {
  auto concatenator = CompressionGroupConcatenator::Create(
      CompressionGroupConcatenator::CompressionType::kUncompressed);
  concatenator->AddCompressionGroup(std::string(kTestString));
  concatenator->AddCompressionGroup(std::string(kTestString2));
  absl::StatusOr<std::string> maybe_output = concatenator->Build();
  EXPECT_TRUE(maybe_output.ok());

  quiche::QuicheDataReader data_reader(*maybe_output);

  for (const auto& test_string : {kTestString, kTestString2}) {
    uint32_t compression_group_size = 0;
    EXPECT_TRUE(data_reader.ReadUInt32(&compression_group_size));
    EXPECT_EQ(compression_group_size, test_string.size());

    std::string_view output;
    EXPECT_TRUE(data_reader.ReadStringPiece(&output, compression_group_size));
    EXPECT_THAT(output, StrEq(test_string));
  }
}

TEST(CompressionBlobReaderTest, Success) {
  auto concatenator = CompressionGroupConcatenator::Create(
      CompressionGroupConcatenator::CompressionType::kUncompressed);
  concatenator->AddCompressionGroup(std::string(kTestString));
  concatenator->AddCompressionGroup(std::string(kTestString2));
  absl::StatusOr<std::string> maybe_output = concatenator->Build();
  EXPECT_TRUE(maybe_output.ok());

  auto blob_reader = CompressedBlobReader::Create(
      CompressionGroupConcatenator::CompressionType::kUncompressed,
      *maybe_output);

  EXPECT_FALSE(blob_reader->IsDoneReading());

  auto maybe_compression_group = blob_reader->ExtractOneCompressionGroup();
  EXPECT_TRUE(maybe_compression_group.ok());
  EXPECT_THAT(*maybe_compression_group, StrEq(kTestString));
  EXPECT_FALSE(blob_reader->IsDoneReading());

  maybe_compression_group = blob_reader->ExtractOneCompressionGroup();
  EXPECT_TRUE(maybe_compression_group.ok());
  EXPECT_THAT(*maybe_compression_group, StrEq(kTestString2));
  EXPECT_TRUE(blob_reader->IsDoneReading());
}

}  // namespace
}  // namespace privacy_sandbox::server_common
