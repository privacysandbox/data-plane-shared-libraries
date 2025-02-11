/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ENCRYPTION_KEY_FETCHER_KEY_FETCHER_UTILS_H_
#define ENCRYPTION_KEY_FETCHER_KEY_FETCHER_UTILS_H_

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace privacy_sandbox::server_common {

// Convert KMS key IDs (hex string) to OHTTP key ID format by reading the first
// two characters of the string (because two hex characters' value range from 0
// to 256).
absl::StatusOr<std::string> ToOhttpKeyId(std::string_view key_id);

}  // namespace privacy_sandbox::server_common

#endif  // ENCRYPTION_KEY_FETCHER_KEY_FETCHER_UTILS_H_
