/*
 * Copyright 2024 Google LLC
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
#ifndef SRC_ROMA_BYOB_UTILITY_UTILS_H_
#define SRC_ROMA_BYOB_UTILITY_UTILS_H_

#include <filesystem>
#include <utility>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "src/roma/byob/config/config.h"

namespace privacy_sandbox::server_common::byob {

absl::Status CreateDirectories(const std::filesystem::path& path);
absl::Status RemoveDirectories(const std::filesystem::path& path);

absl::Status Mount(const char* source, const char* target,
                   const char* filesystemtype, int mountflags);

// Returns false if the calling process does not have CAP_SYS_ADMIN privileges
// to create non-Sandbox mode worker. True otherwise.
bool HasClonePermissionsByobWorker(
    ::privacy_sandbox::server_common::byob::Mode mode);

absl::Status SetupPivotRoot(
    const std::filesystem::path& pivot_root_dir,
    absl::Span<const std::pair<std::filesystem::path, std::filesystem::path>>
        sources_and_targets_read_only,
    bool cleanup_pivot_root_dir = true,
    absl::Span<const std::pair<std::filesystem::path, std::filesystem::path>>
        sources_and_targets_read_and_write = {},
    bool remount_root_as_read_only = true);
}  // namespace privacy_sandbox::server_common::byob

#endif  // SRC_ROMA_BYOB_UTILITY_UTILS_H_
