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

#ifndef SRC_ROMA_BYOB_CONFIG_CONFIG_H_
#define SRC_ROMA_BYOB_CONFIG_CONFIG_H_

#include <cstdint>
#include <string>

#include "src/roma/config/function_binding_object_v2.h"

#ifndef LIB_MOUNTS
#if defined(__aarch64__)
#define LIB_MOUNTS "/lib"
#else
#define LIB_MOUNTS "/lib64"
#endif
#endif /* LIB_MOUNTS */

namespace privacy_sandbox::server_common::byob {

inline constexpr std::string_view kLdLibraryPath =
    "LD_LIBRARY_PATH=" LIB_MOUNTS;

// TODO: b/394338833 - Use a default-deny policy for better security.
inline constexpr std::string_view kSeccompBpfPolicy =
    "KILL_PROCESS { "
    "ptrace, "
    "process_vm_readv, "
    "process_vm_writev } "
    "ERRNO(1) { "
// The following syscalls are not supported by AARCH64 but are supported by
// AMD64 hence can only be denylisted for AMD64. See -
// https://github.com/google/kafel/blob/f6020305eb4f404edcbd0a5543580073544b8ead/src/syscalls/aarch64_syscalls.c
// https://github.com/google/kafel/blob/f6020305eb4f404edcbd0a5543580073544b8ead/src/syscalls/amd64_syscalls.c
#if defined(__x86_64__)
    "alarm, "
    "chmod, "
    "chown, "
    "lchown, "
    "rename, "
#endif
    "fchmod, "
    "fchown, "
    "reboot, "
    "setuid, "
    "setgid, "
    "sched_setaffinity } "
    "DEFAULT ALLOW";
// NOTE: The numbering of these modes is used for microbenchmark upload flow for
// perfgate. Please do not renumber these modes.
enum class Mode {
  kModeGvisorSandbox = 0,
  kModeGvisorSandboxDebug = 1,
  kModeMinimalSandbox = 2,
  kModeNsJailSandbox = 3,
};

inline bool AbslParseFlag(absl::string_view text, Mode* mode,
                          std::string* error) {
  if (text == "gvisor") {
    *mode = Mode::kModeGvisorSandbox;
    return true;
  }
  if (text == "gvisor-debug") {
    *mode = Mode::kModeGvisorSandboxDebug;
    return true;
  }
  if (text == "minimal") {
    *mode = Mode::kModeMinimalSandbox;
    return true;
  }
  if (text == "nsjail") {
    *mode = Mode::kModeNsJailSandbox;
    return true;
  }
  *error = "Supported values: gvisor, gvisor-debug, minimal, nsjail.";
  return false;
}

inline std::string AbslUnparseFlag(Mode mode) {
  switch (mode) {
    case Mode::kModeGvisorSandbox:
      return "gvisor";
    case Mode::kModeGvisorSandboxDebug:
      return "gvisor-debug";
    case Mode::kModeMinimalSandbox:
      return "minimal";
    case Mode::kModeNsJailSandbox:
      return "nsjail";
    default:
      return absl::StrCat(mode);
  }
}

template <typename TMetadata = google::scp::roma::DefaultMetadata>
struct Config {
  std::uint64_t memory_limit_soft = 0;
  std::uint64_t memory_limit_hard = 0;
  std::string roma_container_name;
  // Mounts /x -> /x and /y/z -> /z.
  std::string lib_mounts = LIB_MOUNTS;
  bool enable_seccomp_filter = false;
};
}  // namespace privacy_sandbox::server_common::byob

#endif  // SRC_ROMA_BYOB_CONFIG_CONFIG_H_
