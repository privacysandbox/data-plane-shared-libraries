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

#include "errors.h"

#include "absl/container/flat_hash_map.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::core::errors {

absl::flat_hash_map<uint64_t, absl::flat_hash_map<uint64_t, SCPError>>&
GetGlobalErrorCodes() {
  // Static duration map is heap allocated to avoid destructor call.
  // Defines global_error_codes to store all error codes.
  static auto& global_error_codes =
      *new absl::flat_hash_map<uint64_t,
                               absl::flat_hash_map<uint64_t, SCPError>>();
  return global_error_codes;
}

absl::flat_hash_map<uint64_t, uint64_t>& GetPublicErrorCodesMap() {
  // Defines public_error_codes_map to store error codes and associated public
  // error code.
  // Static duration map is heap allocated to avoid destructor call.
  static auto& public_error_codes_map =
      *new absl::flat_hash_map<uint64_t, uint64_t>();
  return public_error_codes_map;
}

}  // namespace google::scp::core::errors
