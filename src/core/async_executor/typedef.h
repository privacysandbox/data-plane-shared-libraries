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

#ifndef CORE_ASYNC_EXECUTOR_TYPEDEF_H_
#define CORE_ASYNC_EXECUTOR_TYPEDEF_H_

#include <chrono>
#include <limits>

namespace google::scp::core {
// TODO: Make the following configurable.
// The maximum thread count could be set.
inline constexpr size_t kMaxThreadCount = 10000;
/// The maximum queue cap could be set.
inline constexpr size_t kMaxQueueCap = std::numeric_limits<uint64_t>::max();
/// The sleep interval for shutting down threads in milliseconds.
inline constexpr size_t kSleepDurationMs = 10;
/// Indicates an infinite wait time.
inline constexpr std::chrono::nanoseconds kInfiniteWaitDurationNs =
    std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::hours(87600));  // 10 years
}  // namespace google::scp::core

#endif  // CORE_ASYNC_EXECUTOR_TYPEDEF_H_
