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

#ifndef UTIL_RLIMIT_CORE_CONFIG_H_
#define UTIL_RLIMIT_CORE_CONFIG_H_

namespace privacysandbox::server_common {

struct RLimitOptions {
  // By default, for prod builds, core dumps should be disabled.
  bool enable_core_dumps = false;
};

// Used for setting core file size. By default, core file size is set to zero
// disabling core dumps. This is the expected prod behaviour. Note that once
// core dumps have been disabled by a process, it cannot be enabled unless the
// process has {PRIV_SYS_RESOURCE} asserted.
// For details, see -
// https://docs.oracle.com/cd/E86824_01/html/E54765/setrlimit-2.html
void SetRLimits(const RLimitOptions& options = RLimitOptions{});

}  // namespace privacysandbox::server_common
#endif  // UTIL_RLIMIT_CORE_CONFIG_H_
