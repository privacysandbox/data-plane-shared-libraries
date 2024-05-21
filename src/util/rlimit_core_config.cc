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

#include "src/util/rlimit_core_config.h"

#include <sys/resource.h>

#include <cstdint>
#include <cstring>

#include "absl/log/log.h"

namespace privacysandbox::server_common {

namespace {
constexpr uint64_t kNoCoreSize = 0;
constexpr uint64_t kCoreSize = 100'000'000;
}  // namespace

void SetRLimits(const RLimitOptions& options) {
  const uint64_t rlimit_core_val =
      options.enable_core_dumps ? kCoreSize : kNoCoreSize;
  const rlimit rlimit_core = {
      .rlim_cur = rlimit_core_val,
      .rlim_max = rlimit_core_val,
  };
  if (setrlimit(RLIMIT_CORE, &rlimit_core) == -1) {
    LOG(INFO) << "Setting core file size failed. " << std::strerror(errno);
  }
}

}  // namespace privacysandbox::server_common
