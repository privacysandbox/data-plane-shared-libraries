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

#include "logger.h"

#include <cstdarg>
#include <memory>
#include <sstream>
#include <string_view>
#include <utility>

#include "core/common/uuid/src/uuid.h"
#include "core/logger/interface/log_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "scp/cc/core/interface/logger_interface.h"

using google::scp::core::common::Uuid;

namespace {
// Default values are empty to save characters on each log on the wire.
constexpr std::string_view kDefaultMachineName = "";
constexpr std::string_view kDefaultClusterName = "";
}  // namespace

namespace google::scp::core::logger {

ExecutionResult Logger::Init() noexcept {
  return log_provider_->Init();
}

ExecutionResult Logger::Run() noexcept {
  return log_provider_->Run();
}

ExecutionResult Logger::Stop() noexcept {
  return log_provider_->Stop();
}

void Logger::Info(std::string_view component_name, const Uuid& correlation_id,
                  const Uuid& parent_activity_id, const Uuid& activity_id,
                  std::string_view location, std::string_view message,
                  ...) noexcept {
  va_list args;
  va_start(args, message);
  log_provider_->Log(LogLevel::kInfo, correlation_id, parent_activity_id,
                     activity_id, component_name, kDefaultMachineName,
                     kDefaultClusterName, location, message, args);
  va_end(args);
}

void Logger::Debug(std::string_view component_name, const Uuid& correlation_id,
                   const Uuid& parent_activity_id, const Uuid& activity_id,
                   std::string_view location, std::string_view message,
                   ...) noexcept {
  va_list args;
  va_start(args, message);
  log_provider_->Log(LogLevel::kDebug, correlation_id, parent_activity_id,
                     activity_id, component_name, kDefaultMachineName,
                     kDefaultClusterName, location, message, args);
  va_end(args);
}

void Logger::Warning(std::string_view component_name,
                     const Uuid& correlation_id, const Uuid& parent_activity_id,
                     const Uuid& activity_id, std::string_view location,
                     std::string_view message, ...) noexcept {
  va_list args;
  va_start(args, message);
  log_provider_->Log(LogLevel::kWarning, correlation_id, parent_activity_id,
                     activity_id, component_name, kDefaultMachineName,
                     kDefaultClusterName, location, message, args);
  va_end(args);
}

void Logger::Error(std::string_view component_name, const Uuid& correlation_id,
                   const Uuid& parent_activity_id, const Uuid& activity_id,
                   std::string_view location, std::string_view message,
                   ...) noexcept {
  va_list args;
  va_start(args, message);
  log_provider_->Log(LogLevel::kError, correlation_id, parent_activity_id,
                     activity_id, component_name, kDefaultMachineName,
                     kDefaultClusterName, location, message, args);
  va_end(args);
}

void Logger::Alert(std::string_view component_name, const Uuid& correlation_id,
                   const Uuid& parent_activity_id, const Uuid& activity_id,
                   std::string_view location, std::string_view message,
                   ...) noexcept {
  va_list args;
  va_start(args, message);
  log_provider_->Log(LogLevel::kAlert, correlation_id, parent_activity_id,
                     activity_id, component_name, kDefaultMachineName,
                     kDefaultClusterName, location, message, args);
  va_end(args);
}

void Logger::Critical(std::string_view component_name,
                      const Uuid& correlation_id,
                      const Uuid& parent_activity_id, const Uuid& activity_id,
                      std::string_view location, std::string_view message,
                      ...) noexcept {
  va_list args;
  va_start(args, message);
  log_provider_->Log(LogLevel::kCritical, correlation_id, parent_activity_id,
                     activity_id, component_name, kDefaultMachineName,
                     kDefaultClusterName, location, message, args);
  va_end(args);
}

void Logger::Emergency(std::string_view component_name,
                       const Uuid& correlation_id,
                       const Uuid& parent_activity_id, const Uuid& activity_id,
                       std::string_view location, std::string_view message,
                       ...) noexcept {
  va_list args;
  va_start(args, message);
  log_provider_->Log(LogLevel::kEmergency, correlation_id, parent_activity_id,
                     activity_id, component_name, kDefaultMachineName,
                     kDefaultClusterName, location, message, args);
  va_end(args);
}

}  // namespace google::scp::core::logger
