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

#include "uuid.h"

#include <cctype>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

#include "absl/random/random.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "src/public/core/interface/execution_result.h"
#include "src/util/duration.h"

#include "error_codes.h"

namespace google::scp::core::common {

Uuid Uuid::GenerateUuid() noexcept {
  absl::BitGen bitgen;
  return Uuid{
      .high = static_cast<uint64_t>(absl::ToInt64Nanoseconds(
          privacy_sandbox::server_common::CpuThreadTimeStopwatch()
              .GetElapsedTime())),
      .low = absl::Uniform<uint64_t>(bitgen),
  };
}

namespace {

uint64_t ReadHex(std::string_view string_to_read, const int offset) {
  const std::string digits(string_to_read.substr(offset, 2));
  std::istringstream istrstream(digits);
  int byte = 0;
  istrstream >> std::hex >> byte;
  return byte;
}

}  // namespace

std::string ToString(const Uuid& uuid) noexcept {
  // Uuid has two 8-byte variables, high and low. A GUID string can be generated
  // by converting each byte to a hexadecimal value. The GUID format in hex is:
  //   00000000-0000-0000-0000-000000000000
  std::string uuid_str;
  uuid_str.reserve(36);
  absl::StrAppend(&uuid_str, absl::Hex(uuid.high, absl::kZeroPad16),
                  absl::Hex(uuid.low, absl::kZeroPad16));
  absl::AsciiStrToUpper(&uuid_str);
  for (int i : {20, 16, 12, 8}) {
    uuid_str.insert(i, 1, '-');
  }
  return uuid_str;
}

ExecutionResult FromString(std::string_view uuid_string, Uuid& uuid) noexcept {
  if (uuid_string.length() != 36) {
    return FailureExecutionResult(errors::SC_UUID_INVALID_STRING);
  }
  for (size_t i = 0; i < uuid_string.length(); ++i) {
    if (uuid_string[i] == '-') {
      if (!(i == 8 || i == 13 || i == 18 || i == 23)) {
        return FailureExecutionResult(errors::SC_UUID_INVALID_STRING);
      }
      continue;
    }
    if (!std::isxdigit(uuid_string[i])) {
      return FailureExecutionResult(errors::SC_UUID_INVALID_STRING);
    }
    if (std::islower(uuid_string[i])) {
      return FailureExecutionResult(errors::SC_UUID_INVALID_STRING);
    }
  }

  constexpr int shifts[] = {56, 48, 40, 32, 24, 16, 8, 0};
  size_t offset = 0;
  for (auto shift : shifts) {
    uuid.high |= (ReadHex(uuid_string, offset) << shift);
    switch (shift) {
      case 32:
        [[fallthrough]];
      case 16:
        [[fallthrough]];
      case 0:
        offset += 3;
        break;
      default:
        offset += 2;
        break;
    }
  }
  for (auto shift : shifts) {
    uuid.low |= (ReadHex(uuid_string, offset) << shift);
    if (shift == 48) {
      offset += 3;
    } else {
      offset += 2;
    }
  }
  return SuccessExecutionResult();
}

}  // namespace google::scp::core::common
