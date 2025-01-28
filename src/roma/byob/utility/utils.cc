// Copyright 2024 Google LLC
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

#include "src/roma/byob/utility/utils.h"

#include <sys/capability.h>
#include <sys/mount.h>
#include <sys/syscall.h>

#include <algorithm>
#include <filesystem>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "src/roma/byob/config/config.h"
#include "src/util/status_macro/status_macros.h"

namespace privacy_sandbox::server_common::byob {
absl::Status CreateDirectories(const std::filesystem::path& path) {
  std::error_code ec;
  if (std::filesystem::create_directories(path, ec); ec) {
    return absl::InternalError(
        absl::StrCat("Failed to create '", path.native(), "': ", ec.message()));
  }
  return absl::OkStatus();
}

absl::Status RemoveDirectories(const std::filesystem::path& path) {
  if (std::error_code ec; std::filesystem::remove_all(path, ec) ==
                          static_cast<std::uintmax_t>(-1)) {
    return absl::InternalError(absl::StrCat(
        "Failed to remove_all '", path.native(), "': ", ec.message()));
  }
  return absl::OkStatus();
}

absl::Status Mount(const char* source, const char* target,
                   const char* filesystemtype, int mountflags) {
  if (::mount(source, target, filesystemtype, mountflags, nullptr) == -1) {
    return absl::ErrnoToStatus(errno, absl::StrCat("Failed to mount '", source,
                                                   "' to '", target, "'"));
  }
  return absl::OkStatus();
}

using ::privacy_sandbox::server_common::byob::Mode;

bool HasClonePermissionsByobWorker(Mode mode) {
  // For sandbox mode, since runsc container tries to clone workers, it doesn't
  // matter if the calling process had CAP_SYS_ADMIN.
  if (mode == Mode::kModeGvisorSandbox ||
      mode == Mode::kModeGvisorSandboxDebug) {
    return true;
  }
  cap_t capabilities = cap_get_proc();
  cap_flag_value_t value;
  int result = cap_get_flag(capabilities, CAP_SYS_ADMIN, CAP_EFFECTIVE, &value);
  cap_free(capabilities);
  // If cap_get_flag fails, return true and allow the code to run and crash
  // loudly (as needed).
  if (result != 0) {
    PLOG(ERROR) << "Failed to cap_get_flag";
    return true;
  }
  if (value == CAP_SET) {
    LOG(INFO) << "Process has CAP_SYS_ADMIN";
    return true;
  } else {
    LOG(INFO) << "Process does not have CAP_SYS_ADMIN";
    return false;
  }
}

absl::Status SetupPivotRoot(
    const std::filesystem::path& pivot_root_dir,
    absl::Span<const std::pair<std::filesystem::path, std::filesystem::path>>
        sources_and_targets_read_only,
    const bool cleanup_pivot_root_dir,
    absl::Span<const std::pair<std::filesystem::path, std::filesystem::path>>
        sources_and_targets_read_and_write,
    const bool remount_root_as_read_only) {
  if (cleanup_pivot_root_dir) {
    PS_RETURN_IF_ERROR(RemoveDirectories(pivot_root_dir));
  }
  // Set up restricted filesystem for worker using pivot_root
  // pivot_root doesn't work under an MS_SHARED mount point.
  // https://man7.org/linux/man-pages/man2/pivot_root.2.html.
  PS_RETURN_IF_ERROR(Mount(/*source=*/nullptr, /*target=*/"/",
                           /*filesystemtype=*/nullptr, MS_REC | MS_PRIVATE));
  for (const auto& [source, target] : sources_and_targets_read_only) {
    const std::filesystem::path mount_target =
        pivot_root_dir / target.relative_path();
    PS_RETURN_IF_ERROR(CreateDirectories(mount_target));
    PS_RETURN_IF_ERROR(Mount(source.c_str(), mount_target.c_str(),
                             /*filesystemtype=*/nullptr, MS_BIND | MS_RDONLY));
  }
  for (const auto& [source, target] : sources_and_targets_read_and_write) {
    const std::filesystem::path mount_target =
        pivot_root_dir / target.relative_path();
    PS_RETURN_IF_ERROR(CreateDirectories(mount_target));
    PS_RETURN_IF_ERROR(Mount(source.c_str(), mount_target.c_str(),
                             /*filesystemtype=*/nullptr, MS_BIND));
  }

  // MS_REC needed here to get other mounts (/lib, /lib64 etc)
  PS_RETURN_IF_ERROR(Mount(pivot_root_dir.c_str(), pivot_root_dir.c_str(),
                           "bind", MS_REC | MS_BIND));
  {
    const std::filesystem::path pivot_dir = pivot_root_dir / "pivot";
    PS_RETURN_IF_ERROR(CreateDirectories(pivot_dir));
    if (::syscall(SYS_pivot_root, pivot_root_dir.c_str(), pivot_dir.c_str()) ==
        -1) {
      return absl::ErrnoToStatus(
          errno,
          absl::StrCat("syscall(SYS_pivot_root, '", pivot_root_dir.c_str(),
                       "', '", pivot_dir.c_str(), "')"));
    }
  }
  if (::chdir("/") == -1) {
    return absl::ErrnoToStatus(errno, "chdir('/')");
  }
  if (::umount2("/pivot", MNT_DETACH) == -1) {
    return absl::ErrnoToStatus(errno, "mount2('/pivot', MNT_DETACH)");
  }
  if (::rmdir("/pivot") == -1) {
    return absl::ErrnoToStatus(errno, "rmdir('/pivot')");
  }
  for (const auto& [_, target] : sources_and_targets_read_only) {
    PS_RETURN_IF_ERROR(Mount(target.c_str(), target.c_str(),
                             /*filesystemtype=*/nullptr,
                             MS_REMOUNT | MS_BIND | MS_RDONLY));
  }
  for (const auto& [_, target] : sources_and_targets_read_and_write) {
    PS_RETURN_IF_ERROR(Mount(target.c_str(), target.c_str(),
                             /*filesystemtype=*/nullptr, MS_REMOUNT | MS_BIND));
  }
  if (remount_root_as_read_only) {
    PS_RETURN_IF_ERROR(Mount(/*source=*/"/", /*target=*/"/",
                             /*filesystemtype=*/nullptr,
                             MS_REMOUNT | MS_BIND | MS_RDONLY | MS_PRIVATE));
  }
  return absl::OkStatus();
}

}  // namespace privacy_sandbox::server_common::byob
