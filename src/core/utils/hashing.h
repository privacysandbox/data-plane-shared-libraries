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

#ifndef CORE_UTILS_HASHING_H_
#define CORE_UTILS_HASHING_H_

#include <memory>
#include <string>
#include <vector>

#include "src/core/interface/type_def.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::core::utils {
/**
 * @brief Calculates MD5 hash of the input data and sets it in checksum input
 * parameter as a binary string.
 *
 * @param buffer The buffer to calculate the hash.
 * @return ExecutionResultOr<string> The checksum value.
 */
ExecutionResultOr<std::string> CalculateMd5Hash(
    const std::shared_ptr<std::string>& buffer);

// Same as above but accepts a string.
ExecutionResultOr<std::string> CalculateMd5Hash(std::string_view buffer);

// DEPRECATED, please use the above options.
ExecutionResult CalculateMd5Hash(const std::shared_ptr<std::string>& buffer,
                                 std::string& checksum);

// DEPRECATED, please use the above options.
ExecutionResult CalculateMd5Hash(std::string_view buffer,
                                 std::string& checksum);

}  // namespace google::scp::core::utils

#endif  // CORE_UTILS_HASHING_H_
