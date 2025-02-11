// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/encryption/key_fetcher/key_fetcher_utils.h"

#include "absl/strings/numbers.h"

namespace privacy_sandbox::server_common {

absl::StatusOr<std::string> ToOhttpKeyId(std::string_view key_id) {
  if (key_id.length() < 2) {
    return absl::InvalidArgumentError(
        "Key ID less than 2 characters long - unable to parse as valid OHTTP "
        "key ID");
  }

  int32_t out;
  // Key ID is hex encoded. Left shift first char by 4 and OR with the second
  // char.
  (void)absl::SimpleHexAtoi(std::string{key_id.at(0)}, &out);
  int32_t ohttp_key_id = out << 4;

  int32_t out2;
  (void)absl::SimpleHexAtoi(std::string{key_id.at(1)}, &out2);
  ohttp_key_id = (ohttp_key_id | out2);

  return std::to_string(ohttp_key_id);
}

}  // namespace privacy_sandbox::server_common
