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

#ifndef CORE_COMMON_TIME_PROVIDER_TIME_PROVIDER_H_
#define CORE_COMMON_TIME_PROVIDER_TIME_PROVIDER_H_

#include <chrono>

#include "src/core/interface/type_def.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::core::common {
class TimeProvider {
 public:
  /**
   * @brief Get wall-clock (system time) of the system in nanoseconds.
   *
   * @return nanoseconds The timestamp in nanoseconds
   */
  static std::chrono::nanoseconds GetWallTimestampInNanoseconds() {
    return std::chrono::system_clock::now().time_since_epoch();
  }

  /**
   * @brief Get wall-clock (system time) of the system in nanoseconds.
   *
   * @return Timestamp The timestamp in nanoseconds in ticks format
   */
  static Timestamp GetWallTimestampInNanosecondsAsClockTicks() {
    return GetWallTimestampInNanoseconds().count();
  }

  /**
   * @brief Get CPU ticks elapsed since the last reboot in nanoseconds. This
   * is used to measure time intervals.
   *
   * @return nanoseconds The timestamp in nanoseconds
   */
  static std::chrono::nanoseconds GetSteadyTimestampInNanoseconds() {
    return std::chrono::steady_clock::now().time_since_epoch();
  }

  /**
   * @brief Get CPU ticks elapsed since the last reboot in nanoseconds. This
   * is used to measure time intervals.
   *
   * @return Timestamp The timestamp in nanoseconds in ticks format
   */
  static Timestamp GetSteadyTimestampInNanosecondsAsClockTicks() {
    return GetSteadyTimestampInNanoseconds().count();
  }
};

};  // namespace google::scp::core::common

#endif  // CORE_COMMON_TIME_PROVIDER_TIME_PROVIDER_H_
