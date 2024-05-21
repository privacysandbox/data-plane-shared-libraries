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

#include "src/communication/json_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "google/protobuf/struct.pb.h"
#include "google/protobuf/text_format.h"

namespace privacy_sandbox::server_common {
namespace {

using google::protobuf::Struct;
using ::testing::StrEq;

TEST(BinaryHttpTest, JsonToProtoSuccess) {
  const auto maybe_proto = JsonToProto<Struct>(R"({"key": "value"})");
  ASSERT_TRUE(maybe_proto.ok());
  EXPECT_THAT(maybe_proto->fields().at("key").string_value(), StrEq("value"));
}

TEST(BinaryHttpTest, JsonToProto_MalformedJson) {
  const auto maybe_proto = JsonToProto<Struct>(R"({"key": "value"}}}})");
  ASSERT_TRUE(!maybe_proto.ok());
  ASSERT_TRUE(absl::IsInvalidArgument(maybe_proto.status()));
}

TEST(BinaryHttpTest, ProtoToJsonSuccess) {
  Struct struct_proto;
  (*struct_proto.mutable_fields())["key"].set_string_value("value");

  auto maybe_json = ProtoToJson<Struct>(struct_proto);
  ASSERT_TRUE(maybe_json.ok());
}

}  // namespace
}  // namespace privacy_sandbox::server_common
