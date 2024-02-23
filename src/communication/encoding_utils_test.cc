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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"

using ::testing::StrEq;

namespace privacy_sandbox::server_common {
namespace {

// The tests below define use absl's BytesToHexString() and HexToByteString()
// methods to transform the expected input/outputs to the methods under test, so
// it's easier to debug/understand.

TEST(EncodingUtilsTest, EncodeResponsePayloadSuccess) {
  const std::string compressed_payload = "payload";
  const absl::StatusOr<std::string> actual =
      EncodeResponsePayload(CompressionType::kBrotli, compressed_payload, 128);

  // The expected result should follow this format:
  // version/compression - compressed data length - data           - padding
  // 01                  - 00000007               - 7061796c6f6164 - 000...
  const std::string expected = "01000000077061796c6f6164";

  EXPECT_THAT(
      absl::BytesToHexString(actual.value()).substr(0, expected.length()),
      StrEq(expected));
  // The actual value should be encoded and padded to 128 bytes.
  EXPECT_EQ(actual.value().size(), 128);
}

TEST(EncodingUtilsTest, EncodeResponsePayloadFailure_PayloadTooLargeToEncode) {
  const std::string compressed_payload(128, 'q');
  const absl::StatusOr<std::string> actual =
      EncodeResponsePayload(CompressionType::kBrotli, compressed_payload, 128);

  ASSERT_TRUE(!actual.ok());
  ASSERT_TRUE(absl::IsInternal(actual.status()));
}

TEST(EncodingUtilsTest, DecodeMaxVersionMaxZipPayloadSuccess) {
  const std::string expected_compressed_message = "payload";
  const std::string encoded_payload = "FF000000077061796c6f6164";
  const absl::StatusOr<DecodedRequest> decoded_payload(
      DecodeRequestPayload(absl::HexStringToBytes(encoded_payload)));

  EXPECT_EQ(decoded_payload->framing_version, 7);
  EXPECT_EQ(static_cast<int>(decoded_payload->compression_type), 31);
  EXPECT_THAT(decoded_payload->compressed_data,
              StrEq(expected_compressed_message));
}

TEST(EncodingUtilsTest, DecodeRequestPayloadSuccess_NoPadding) {
  const std::string expected_compressed_message = "payload";
  const std::string encoded_payload = "01000000077061796c6f6164";
  const absl::StatusOr<DecodedRequest> decoded_payload(
      DecodeRequestPayload(absl::HexStringToBytes(encoded_payload)));

  EXPECT_EQ(decoded_payload->framing_version, 0);
  EXPECT_EQ(decoded_payload->compression_type, CompressionType::kBrotli);
  EXPECT_THAT(decoded_payload->compressed_data,
              StrEq(expected_compressed_message));
}

TEST(EncodingUtilsTest, DecodeRequestPayloadSuccess_WithPadding) {
  const std::string expected_compressed_message = "payload";
  const std::string encoded_payload = "01000000077061796c6f61640000";
  const absl::StatusOr<DecodedRequest> decoded_payload(
      DecodeRequestPayload(absl::HexStringToBytes(encoded_payload)));

  EXPECT_EQ(decoded_payload->framing_version, 0);
  EXPECT_EQ(decoded_payload->compression_type, CompressionType::kBrotli);
  EXPECT_THAT(decoded_payload->compressed_data,
              StrEq(expected_compressed_message));
}

TEST(EncodingUtilsTest, DecodeRequestPayloadFailure_MalformedPayload) {
  // This test deletes the last char and padding from the encoded payload in the
  // above tests, so the 4 bits that indicate the size of the compressed data is
  // larger than how many bits follow it in the payload.
  const std::string encoded_payload = "01000000077061796c6f616";
  const absl::StatusOr<DecodedRequest> decoded_payload(
      DecodeRequestPayload(absl::HexStringToBytes(encoded_payload)));

  ASSERT_TRUE(!decoded_payload.ok());
  ASSERT_TRUE(absl::IsInvalidArgument(decoded_payload.status()));
}

}  // namespace
}  // namespace privacy_sandbox::server_common
