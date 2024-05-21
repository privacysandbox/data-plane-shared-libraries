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

#ifndef CORE_COMMON_UUID_TEST_UUID_TO_STRING_H_
#define CORE_COMMON_UUID_TEST_UUID_TO_STRING_H_

#include <string>

#include "absl/functional/any_invocable.h"
#include "src/core/common/uuid/uuid.h"

namespace google::scp::core::common::test {

std::string ToStringAbslFormat(
    const google::scp::core::common::Uuid& uuid) noexcept;

std::string ToStringAbslAppend(
    const google::scp::core::common::Uuid& uuid) noexcept;

void AppendHexByte(int byte, std::string& string_to_append) noexcept;

void AppendHexLookupMap(int byte, std::string& string_to_append) noexcept;

std::string ToStringFn(
    const google::scp::core::common::Uuid& uuid,
    absl::AnyInvocable<void(int, std::string&)> AppendHex) noexcept;

}  // namespace google::scp::core::common::test

#endif  // CORE_COMMON_UUID_TEST_UUID_TO_STRING_H_
