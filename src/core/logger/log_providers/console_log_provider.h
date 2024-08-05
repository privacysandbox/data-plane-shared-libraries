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

#ifndef CORE_LOGGER_LOG_PROVIDERS_CONSOLE_LOG_PROVIDER_H_
#define CORE_LOGGER_LOG_PROVIDERS_CONSOLE_LOG_PROVIDER_H_

#include <string>
#include <string_view>

#include "src/core/common/uuid/uuid.h"
#include "src/core/logger/interface/log_provider_interface.h"

namespace google::scp::core::logger {
/**
 * @brief Logs messages to the console.
 *
 */
class ConsoleLogProvider : public LogProviderInterface {
 public:
  virtual ~ConsoleLogProvider() = default;
  void Log(const LogLevel& level, const common::Uuid& correlation_id,
           const common::Uuid& parent_activity_id,
           const common::Uuid& activity_id, std::string_view component_name,
           std::string_view location, std::string_view message,
           ...) noexcept override;

 protected:
  virtual void Print(std::string_view output) noexcept;
};
}  // namespace google::scp::core::logger

#endif  // CORE_LOGGER_LOG_PROVIDERS_CONSOLE_LOG_PROVIDER_H_
