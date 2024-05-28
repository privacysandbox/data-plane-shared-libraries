/*
 * Copyright 2022 Google LLC
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

//
// alternative implementations of the Uuid ToString function, for benchmarking
//

#include "uuid_to_string.h"

#include <string>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "src/core/common/uuid/error_codes.h"
#include "src/core/common/uuid/uuid.h"

namespace google::scp::core::common::test {

using google::scp::core::common::Uuid;

std::string ToStringAbslFormat(const Uuid& uuid) noexcept {
  std::string uuid_string;
  uuid_string.reserve(36);
  absl::StrAppendFormat(&uuid_string, "%08X", uuid.high >> 32 & 0xFFFFFFFF);
  absl::StrAppend(&uuid_string, "-");
  absl::StrAppendFormat(&uuid_string, "%04X", uuid.high >> 16 & 0xFFFF);
  absl::StrAppend(&uuid_string, "-");
  absl::StrAppendFormat(&uuid_string, "%04X", uuid.high & 0xFFFF);
  absl::StrAppend(&uuid_string, "-");
  absl::StrAppendFormat(&uuid_string, "%04X", uuid.low >> 48 & 0xFFFF);
  absl::StrAppend(&uuid_string, "-");
  absl::StrAppendFormat(&uuid_string, "%012X", uuid.low & 0xFFFFFFFFFFFF);
  return uuid_string;
}

std::string ToStringAbslAppend(const Uuid& uuid) noexcept {
  std::string uuid_string;
  uuid_string.reserve(36);
  absl::StrAppend(&uuid_string,
                  absl::Hex(uuid.high >> 32 & 0xFFFFFFFF, absl::kZeroPad8));
  absl::StrAppend(&uuid_string, "-");
  absl::StrAppend(&uuid_string,
                  absl::Hex(uuid.high >> 16 & 0xFFFF, absl::kZeroPad4));
  absl::StrAppend(&uuid_string, "-");
  absl::StrAppend(&uuid_string, absl::Hex(uuid.high & 0xFFFF, absl::kZeroPad4));
  absl::StrAppend(&uuid_string, "-");
  absl::StrAppend(&uuid_string,
                  absl::Hex(uuid.low >> 48 & 0xFFFF, absl::kZeroPad4));
  absl::StrAppend(&uuid_string, "-");
  absl::StrAppend(&uuid_string,
                  absl::Hex(uuid.low & 0xFFFFFFFFFFFF, absl::kZeroPad12));
  absl::AsciiStrToUpper(&uuid_string);
  return uuid_string;
}

void AppendHexByte(int byte, std::string& string_to_append) noexcept {
  absl::StrAppend(&string_to_append, absl::Hex(byte, absl::kZeroPad2));
}

constexpr std::string_view kHexMap = "0123456789ABCDEF";

void AppendHexLookupMap(int byte, std::string& string_to_append) noexcept {
  // construct a null-terminated string
  const char hexstr[] = {
      kHexMap[byte >> 4],    // first_digit
      kHexMap[byte & 0x0F],  // second_digit
      0x0,
  };
  absl::StrAppend(&string_to_append, hexstr);
}

std::string ToStringFn(
    const Uuid& uuid,
    absl::AnyInvocable<void(int, std::string&)> AppendHex) noexcept {
  std::string uuid_string;
  uuid_string.reserve(36);

  constexpr int shifts[] = {56, 48, 40, 32, 24, 16, 8, 0};

  for (int shift : shifts) {
    AppendHex((uuid.high >> shift) & 0xFF, uuid_string);
    switch (shift) {
      case 32:
        [[fallthrough]];
      case 16:
        [[fallthrough]];
      case 0:
        absl::StrAppend(&uuid_string, "-");
        break;
      default:
        break;
    }
  }
  for (int shift : shifts) {
    AppendHex((uuid.low >> shift) & 0xFF, uuid_string);
    if (shift == 48) {
      absl::StrAppend(&uuid_string, "-");
    }
  }
  absl::AsciiStrToUpper(&uuid_string);
  return uuid_string;
}

}  // namespace google::scp::core::common::test
