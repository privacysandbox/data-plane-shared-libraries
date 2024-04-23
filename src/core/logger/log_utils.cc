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
#include "src/core/logger/log_utils.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "src/core/logger/interface/log_provider_interface.h"

namespace google::scp::core::logger {

std::string ToString(const LogLevel& level) {
  switch (level) {
    case LogLevel::kEmergency:
      return "Emergency";
    case LogLevel::kAlert:
      return "Alert";
    case LogLevel::kCritical:
      return "Critical";
    case LogLevel::kDebug:
      return "Debug";
    case LogLevel::kInfo:
      return "Info";
    case LogLevel::kWarning:
      return "Warning";
    case LogLevel::kError:
      return "Error";
    case LogLevel::kNone:
      return "None";
  }
  return "Unknown";
}

LogLevel FromString(std::string_view level) {
  if (level == "Emergency") {
    return LogLevel::kEmergency;
  }
  if (level == "Alert") {
    return LogLevel::kAlert;
  }
  if (level == "Critical") {
    return LogLevel::kCritical;
  }
  if (level == "Debug") {
    return LogLevel::kDebug;
  }
  if (level == "Info") {
    return LogLevel::kInfo;
  }
  if (level == "Warning") {
    return LogLevel::kWarning;
  }
  if (level == "Error") {
    return LogLevel::kError;
  }

  return LogLevel::kNone;
}

std::string operator+(const LogLevel& level, std::string_view text) {
  return absl::StrCat(ToString(level), text);
}

std::string operator+(std::string_view text, const LogLevel& level) {
  return absl::StrCat(text, ToString(level));
}

}  // namespace google::scp::core::logger
