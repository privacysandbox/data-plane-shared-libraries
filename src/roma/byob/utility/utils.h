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
        sources_and_targets,
    bool cleanup_pivot_root_dir = true);
}  // namespace privacy_sandbox::server_common::byob
