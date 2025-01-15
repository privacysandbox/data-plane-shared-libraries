/*
 * Copyright 2024 Google LLC
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

#include "src/logger/request_context_logger.h"

namespace privacy_sandbox::server_common::log {
namespace {
// Though atomic in general should be avoided
// https://abseil.io/docs/cpp/atomic_danger, we follow the atomic safety
// guidelines to leverage std::atomic to achieve lock-free
// performance for logging verbosity reads.
//
// We can safely use atomic here because of the following:
// 1. The max verbosity is a simple integer
// 2. In the verbosity update operation, the value will be set at most once
// by a single thread
// 3. The read of the verbosity value is independent from other mutex guarded
// data
// 4. The race condition for logging with different verbosity levels is
// inconsequential
//
// Other use case of atomic should be carefully evaluated, and
// alternative solutions in https://abseil.io/docs/cpp/atomic_danger are
// highly preferred.
ABSL_CONST_INIT std::atomic<int> max_verbosity(0);
}  // namespace

void SetGlobalPSVLogLevel(int verbosity_level) {
  if (verbosity_level < 0) {
    fprintf(
        stderr,
        "Warning: max verbosity cannot be set with negative verbosity level "
        "%d.\n",
        verbosity_level);
    return;
  }
  if (const int max_v = max_verbosity.load(std::memory_order_relaxed);
      max_v != verbosity_level) {
    max_verbosity.store(verbosity_level, std::memory_order_relaxed);
  }
}

bool PS_VLOG_IS_ON(int verbosity_level) {
  const int max_v = max_verbosity.load(std::memory_order_relaxed);
  return verbosity_level <= max_v;
}
}  // namespace privacy_sandbox::server_common::log
