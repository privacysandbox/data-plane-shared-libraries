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

enum class Mode {
  kModeSandbox,
  kModeNoSandbox,
  kModeSandboxDebug,
};

inline bool AbslParseFlag(absl::string_view text, Mode* mode,
                          std::string* error) {
  if (text == "on") {
    *mode = Mode::kModeSandbox;
    return true;
  }
  if (text == "debug") {
    *mode = Mode::kModeSandboxDebug;
    return true;
  }
  if (text == "off") {
    *mode = Mode::kModeNoSandbox;
    return true;
  }
  *error = "Supported values: on, off, debug.";
  return false;
}

inline std::string AbslUnparseFlag(Mode mode) {
  switch (mode) {
    case Mode::kModeSandbox:
      return "on";
    case Mode::kModeSandboxDebug:
      return "debug";
    case Mode::kModeNoSandbox:
      return "off";
    default:
      return absl::StrCat(mode);
  }
}

template <typename TMetadata = google::scp::roma::DefaultMetadata>
struct Config {
  std::uint64_t memory_limit_soft = 0;
  std::uint64_t memory_limit_hard = 0;
  std::string roma_container_name;
  std::string lib_mounts = LIB_MOUNTS;
};
}  // namespace privacy_sandbox::server_common::byob

#endif  // SRC_ROMA_BYOB_CONFIG_CONFIG_H_
