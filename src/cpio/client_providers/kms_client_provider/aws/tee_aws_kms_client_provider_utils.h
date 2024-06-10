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

#ifndef CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_AWS_TEE_AWS_KMS_CLIENT_PROVIDER_UTILS_H_
#define CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_AWS_TEE_AWS_KMS_CLIENT_PROVIDER_UTILS_H_

#include <string>
#include <string_view>

#include "absl/status/statusor.h"
#include "absl/types/span.h"

namespace google::scp::cpio::client_providers::utils {

/**
 * @brief Executes a command with arguments and returns its output.
 *
 * @param args a null terminated span of C-style strings where the args[0] is
 * the command to run and args[i > 0] are arguments.
 * @return the standard output.
 */
absl::StatusOr<std::string> Exec(absl::Span<const char* const> args) noexcept;

}  // namespace google::scp::cpio::client_providers::utils

#endif  // CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_AWS_TEE_AWS_KMS_CLIENT_PROVIDER_UTILS_H_
