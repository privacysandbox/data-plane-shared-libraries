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

#ifndef CORE_LOGGER_INTERFACE_LOG_PROVIDER_INTERFACE_H_
#define CORE_LOGGER_INTERFACE_LOG_PROVIDER_INTERFACE_H_

#include <cstdarg>
#include <string_view>

#include "src/core/common/uuid/uuid.h"
#include "src/core/interface/service_interface.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::core::logger {

enum class LogLevel {
  kEmergency = 0,
  kAlert = 1,
  kCritical = 2,
  kError = 4,
  kWarning = 8,
  kDebug = 16,
  kInfo = 32,
  kNone = 64
};

/**
 * @brief The LogProviderInterface is implemented by classes that log messages
 * to a specific destination.
 *
 */
class LogProviderInterface {
 public:
  virtual ~LogProviderInterface() = default;

  /**
   * @brief Logs a formatted message to the underlying provider.
   *
   * @param level The severity level of the message.
   * @param parent_activity_id The activity id associated with the message.
   * @param activity_id The activity id associated with the message.
   * @param correlation_id The correlation id associated with the message.
   * @param component_name The name of the component logging the message.
   * @param machine_name The name of the machine logging the message.
   * @param cluster_name The name of the machine cluster logging the
   * message.
   * @param location The file, function, and line where the log was
   * triggered.
   * @param message The message that gets logged.
   * @param ... A set of strings to be formatted into the message.
   */
  virtual void Log(const LogLevel& level,
                   const common::Uuid& parent_activity_id,
                   const common::Uuid& activity_id,
                   const common::Uuid& correlation_id,
                   std::string_view component_name, std::string_view location,
                   std::string_view message, ...) noexcept = 0;
};
}  // namespace google::scp::core::logger

#endif  // CORE_LOGGER_INTERFACE_LOG_PROVIDER_INTERFACE_H_
