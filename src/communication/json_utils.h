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

#ifndef COMMUNICATION_JSON_UTILS_H_
#define COMMUNICATION_JSON_UTILS_H_

#include <string>
#include <string_view>
#include <utility>

#include "absl/status/statusor.h"
#include "google/protobuf/util/json_util.h"

namespace privacy_sandbox::server_common {

// Converts JSON string to proto. JsonToProto() is meant for converting client
// requests to a server defined proto.
//
// InvalidArgumentError will be returned if the JSON is malformed or cannot be
// converted to the specified proto, implying the client sent bad request.
template <typename ProtoMessage>
absl::StatusOr<ProtoMessage> JsonToProto(std::string_view json) {
  static_assert(std::is_base_of<google::protobuf::Message, ProtoMessage>::value,
                "JsonToProto only decodes to protobuf messages.");

  ProtoMessage result;
  google::protobuf::util::JsonParseOptions options;
  options.ignore_unknown_fields = true;
  if (const auto s =
          google::protobuf::util::JsonStringToMessage(json, &result, options);
      !s.ok()) {
    return s;
  }
  return result;
}

// Converts a proto to a JSON string.
template <typename ProtoMessage>
absl::StatusOr<std::string> ProtoToJson(
    const ProtoMessage& proto,
    google::protobuf::util::JsonPrintOptions options = {
        .add_whitespace = false,
    }) {
  static_assert(std::is_base_of<google::protobuf::Message, ProtoMessage>::value,
                "ProtoToJson only encodes from protobuf messages.");

  std::string body;
  if (const auto s =
          google::protobuf::util::MessageToJsonString(proto, &body, options);
      !s.ok()) {
    return s;
  }
  return body;
}

}  // namespace privacy_sandbox::server_common

#endif  // COMMUNICATION_JSON_UTILS_H_
