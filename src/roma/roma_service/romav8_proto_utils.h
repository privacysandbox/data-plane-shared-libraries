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

#ifndef ROMA_ROMA_SERVICE_ROMAV8_PROTO_UTILS_H_
#define ROMA_ROMA_SERVICE_ROMAV8_PROTO_UTILS_H_

#include <string>
#include <utility>

#include <google/protobuf/message_lite.h>
#include <google/protobuf/util/json_util.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace google::scp::roma::romav8 {

template <typename T>
absl::StatusOr<std::string> Encode(const T& obj) {
  static_assert(std::is_base_of<google::protobuf::MessageLite, T>::value,
                "T must be derived from google::protobuf::MessageLite");
  if (std::string s; obj.SerializeToString(&s)) {
    return s;
  }
  return absl::UnknownError("unable to serialize protobuf object");
}

template <typename T>
absl::Status Decode(const std::string& encoded, T& decoded) {
  static_assert(std::is_base_of<google::protobuf::MessageLite, T>::value,
                "T must be derived from google::protobuf::MessageLite");
  if (T obj; obj.ParseFromString(encoded)) {
    decoded.CheckTypeAndMergeFrom(obj);
    return absl::OkStatus();
  }
  return absl::UnknownError("unable to parse protobuf object");
}
}  // namespace google::scp::roma::romav8

#endif  // ROMA_ROMA_SERVICE_ROMAV8_PROTO_UTILS_H_
