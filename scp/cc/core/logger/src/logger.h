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

#ifndef CORE_LOGGER_SRC_LOGGER_H_
#define CORE_LOGGER_SRC_LOGGER_H_

#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "core/common/uuid/src/uuid.h"
#include "core/interface/logger_interface.h"
#include "core/logger/interface/log_provider_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::logger {

/*! @copydoc LoggerInterface
 */
class Logger : public LoggerInterface {
 public:
  /// Constructs a new Logger object
  explicit Logger(std::unique_ptr<LogProviderInterface> log_provider)
      : log_provider_(std::move(log_provider)) {}

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  void Info(std::string_view component_name, const common::Uuid& correlation_id,
            const common::Uuid& parent_activity_id,
            const common::Uuid& activity_id, std::string_view location,
            std::string_view message, ...) noexcept override;

  void Debug(std::string_view component_name,
             const common::Uuid& correlation_id,
             const common::Uuid& parent_activity_id,
             const common::Uuid& activity_id, std::string_view location,
             std::string_view message, ...) noexcept override;

  void Warning(std::string_view component_name,
               const common::Uuid& correlation_id,
               const common::Uuid& parent_activity_id,
               const common::Uuid& activity_id, std::string_view location,
               std::string_view message, ...) noexcept override;

  void Error(std::string_view component_name,
             const common::Uuid& correlation_id,
             const common::Uuid& parent_activity_id,
             const common::Uuid& activity_id, std::string_view location,
             std::string_view message, ...) noexcept override;

  void Alert(std::string_view component_name,
             const common::Uuid& correlation_id,
             const common::Uuid& parent_activity_id,
             const common::Uuid& activity_id, std::string_view location,
             std::string_view message, ...) noexcept override;

  void Critical(std::string_view component_name,
                const common::Uuid& correlation_id,
                const common::Uuid& parent_activity_id,
                const common::Uuid& activity_id, std::string_view location,
                std::string_view message, ...) noexcept override;

  void Emergency(std::string_view component_name,
                 const common::Uuid& correlation_id,
                 const common::Uuid& parent_activity_id,
                 const common::Uuid& activity_id, std::string_view location,
                 std::string_view message, ...) noexcept override;

 protected:
  /// A unique pointer to the log provider instance.
  std::unique_ptr<logger::LogProviderInterface> log_provider_;
};
}  // namespace google::scp::core::logger

#endif  // CORE_LOGGER_SRC_LOGGER_H_
