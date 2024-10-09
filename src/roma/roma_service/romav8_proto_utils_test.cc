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

#include "src/roma/roma_service/romav8_proto_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/roma/roma_service/helloworld.pb.h"

namespace google::scp::roma::romav8 {

template <>
absl::Status Decode(const std::string& encoded, std::string& decoded) {
  decoded = encoded;
  return absl::OkStatus();
}

template <>
absl::StatusOr<std::string> Encode(const std::string& obj) {
  return obj;
}

}  // namespace google::scp::roma::romav8

namespace google::scp::roma::test {
namespace {
using ::romav8::app_api::test::HelloWorldRequest;
}  // namespace

TEST(RomaV8ProtoUtilsTest, EncodeDecodeProtobuf) {
  HelloWorldRequest req;
  req.set_name("Foobar");

  // Encode protobuf message as a string.
  absl::StatusOr<std::string> encoded = google::scp::roma::romav8::Encode(req);
  EXPECT_TRUE(encoded.ok());
  std::string decoded;
  // Decode string to string (no-op).
  EXPECT_TRUE(google::scp::roma::romav8::Decode<>(*encoded, decoded).ok());
  // Encode string as a string (no-op).
  absl::StatusOr<std::string> encoded2 =
      google::scp::roma::romav8::Encode(decoded);
  EXPECT_TRUE(encoded2.ok());
  EXPECT_THAT(*encoded, testing::StrEq(*encoded2));
}
}  // namespace google::scp::roma::test
