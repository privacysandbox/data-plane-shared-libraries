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

#include "syslog_log_provider.h"

#include <stdio.h>
#include <syslog.h>

#include <csignal>
#include <cstdarg>
#include <iostream>
#include <string>
#include <string_view>
#include <tuple>

#include "absl/strings/str_join.h"
#include "src/core/common/uuid/uuid.h"

using google::scp::core::common::ToString;
using google::scp::core::common::Uuid;

namespace google::scp::core::logger::log_providers {
void SyslogLogProvider::Log(const LogLevel& level, const Uuid& correlation_id,
                            const Uuid& parent_activity_id,
                            const Uuid& activity_id,
                            std::string_view component_name,
                            std::string_view location, std::string_view message,
                            ...) noexcept {
  const std::string formatted_message =
      absl::StrJoin(std::make_tuple(component_name, ToString(correlation_id),
                                    ToString(parent_activity_id),
                                    ToString(activity_id), location, message),
                    "|");

  try {
    constexpr std::string_view kLogChannel = "scp-log";
    openlog(kLogChannel.data(), LOG_CONS | LOG_NDELAY, LOG_USER);
  } catch (...) {
    std::cerr << "Error opening connection to syslog.";
    return;
  }
  va_list args;
  va_start(args, message);
  try {
    switch (level) {
      case LogLevel::kDebug:
        vsyslog(LOG_DEBUG, formatted_message.c_str(), args);
        break;
      case LogLevel::kInfo:
        vsyslog(LOG_INFO, formatted_message.c_str(), args);
        break;
      case LogLevel::kWarning:
        vsyslog(LOG_WARNING, formatted_message.c_str(), args);
        break;
      case LogLevel::kError:
        vsyslog(LOG_ERR, formatted_message.c_str(), args);
        break;
      case LogLevel::kAlert:
        vsyslog(LOG_ALERT, formatted_message.c_str(), args);
        break;
      case LogLevel::kEmergency:
        vsyslog(LOG_EMERG, formatted_message.c_str(), args);
        break;
      case LogLevel::kCritical:
        vsyslog(LOG_CRIT, formatted_message.c_str(), args);
        break;
      case LogLevel::kNone:
        std::cerr << "Invalid log type";
        break;
    }
  } catch (...) {
    // TODO: Add code to get exception message
    std::cerr << "Exception thrown while writing to syslog";
  }
  va_end(args);
  try {
    closelog();
  } catch (...) {
    std::cerr << "Error closing connection to syslog.";
  }
}
}  // namespace google::scp::core::logger::log_providers
