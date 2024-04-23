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
#include "console_log_provider.h"

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "src/core/common/time_provider/time_provider.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/logger/log_utils.h"

using google::scp::core::common::TimeProvider;
using google::scp::core::common::ToString;
using google::scp::core::common::Uuid;

constexpr size_t kNanoSecondsMultiplier = (1000 * 1000 * 1000);

namespace google::scp::core::logger {

void ConsoleLogProvider::Log(const LogLevel& level, const Uuid& correlation_id,
                             const Uuid& parent_activity_id,
                             const Uuid& activity_id,
                             std::string_view component_name,
                             std::string_view location,
                             std::string_view message, ...) noexcept {
  const auto current_timestamp =
      TimeProvider::GetWallTimestampInNanosecondsAsClockTicks();
  const auto current_timestamp_seconds =
      current_timestamp / kNanoSecondsMultiplier;
  const auto remainder_nano_seconds =
      (current_timestamp % kNanoSecondsMultiplier);
  std::stringstream output;
  output << current_timestamp_seconds << "." << remainder_nano_seconds << "|"
         << component_name << "|" << ToString(correlation_id) << "|"
         << ToString(parent_activity_id) << "|" << ToString(activity_id) << "|"
         << location << "|" << static_cast<int>(level) << ": ";

  va_list args;
  va_start(args, message);
  va_list size_args;
  va_copy(size_args, args);
  const auto size = std::vsnprintf(nullptr, 0U, message.data(), size_args);
  auto output_message = std::vector<char>(size + 1);
  // vsnprintf adds a terminator at the end, so we need to specify size + 1
  // here.
  std::vsnprintf(&output_message[0], size + 1, message.data(), args);
  va_end(args);

  output << std::string(output_message.data(), size);

  Print(output.str());
}

void ConsoleLogProvider::Print(std::string_view output) noexcept {
  std::cout << output << std::endl;
}
}  // namespace google::scp::core::logger
