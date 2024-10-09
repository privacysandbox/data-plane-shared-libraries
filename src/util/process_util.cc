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

#include "src/util/process_util.h"

#include <unistd.h>

#include <linux/limits.h>

#include <string>

#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"

namespace privacy_sandbox::server_common {

absl::StatusOr<std::string> GetExePath() {
  std::string my_path;
  my_path.reserve(PATH_MAX);
  if (const std::string proc_exe_path =
          absl::StrCat("/proc/", ::getpid(), "/exe");
      readlink(proc_exe_path.c_str(), my_path.data(), PATH_MAX) == -1) {
    return absl::ErrnoToStatus(errno, "Unable to resolve prod pid exe path");
  }
  return my_path;
}

}  // namespace privacy_sandbox::server_common
