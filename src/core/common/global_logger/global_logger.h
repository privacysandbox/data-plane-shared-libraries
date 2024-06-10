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

#ifndef CORE_COMMON_GLOBAL_LOGGER_GLOBAL_LOGGER_H_
#define CORE_COMMON_GLOBAL_LOGGER_GLOBAL_LOGGER_H_

#include <cstdarg>
#include <memory>
#include <string>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "src/core/interface/errors.h"
#include "src/core/logger/interface/log_provider_interface.h"

namespace google::scp::core::common {
enum class LogOption {
  /// Logs into container for test purposes.
  kMock = 0,
  /// Doesn't produce logs in CPIO.
  kNoLog = 1,
  /// Produces logs to console.
  kConsoleLog = 2,
  /// Produces logs to SysLog.
  kSysLog = 3,
};

void InitializeCpioLog(LogOption option);

namespace internal::cpio_log {
::absl::Nullable<logger::LogProviderInterface*> GetLogger();
std::string_view GetErrorMessage(const absl::Status& status);
std::string_view GetErrorMessage(const ExecutionResult& result);
}  // namespace internal::cpio_log
}  // namespace google::scp::core::common

#define SCP_LOCATION absl::StrCat(__FILE__, ":", __func__, ":", __LINE__)

#define SCP_INFO(component_name, activity_id, message, ...)             \
  __SCP_LOG(google::scp::core::logger::LogLevel::kInfo, component_name, \
            activity_id, message, ##__VA_ARGS__)

#define SCP_INFO_CONTEXT(component_name, async_context, message, ...) \
  __SCP_LOG_CONTEXT(google::scp::core::logger::LogLevel::kInfo,       \
                    component_name, async_context, message, ##__VA_ARGS__)

#define SCP_DEBUG(component_name, activity_id, message, ...)             \
  __SCP_LOG(google::scp::core::logger::LogLevel::kDebug, component_name, \
            activity_id, message, ##__VA_ARGS__)

#define SCP_DEBUG_CONTEXT(component_name, async_context, message, ...) \
  __SCP_LOG_CONTEXT(google::scp::core::logger::LogLevel::kDebug,       \
                    component_name, async_context, message, ##__VA_ARGS__)

#define SCP_WARNING(component_name, activity_id, message, ...)             \
  __SCP_LOG(google::scp::core::logger::LogLevel::kWarning, component_name, \
            activity_id, message, ##__VA_ARGS__)

#define SCP_WARNING_CONTEXT(component_name, async_context, message, ...) \
  __SCP_LOG_CONTEXT(google::scp::core::logger::LogLevel::kWarning,       \
                    component_name, async_context, message, ##__VA_ARGS__)

#define __SCP_LOG(log_level, component_name, activity_id, message, ...)      \
  __SCP_LOG_IMPL(log_level, component_name,                                  \
                 google::scp::core::common::kZeroUuid,                       \
                 google::scp::core::common::kZeroUuid, activity_id, message, \
                 ##__VA_ARGS__)

#define __SCP_LOG_CONTEXT(log_level, component_name, async_context, message,  \
                          ...)                                                \
  __SCP_LOG_IMPL(log_level, component_name, async_context.correlation_id,     \
                 async_context.parent_activity_id, async_context.activity_id, \
                 message, ##__VA_ARGS__)

#define SCP_ERROR(component_name, activity_id, execution_result, message, ...) \
  __SCP_LOG_FAIL(google::scp::core::logger::LogLevel::kError, component_name,  \
                 activity_id, execution_result, message, ##__VA_ARGS__)

#define SCP_ERROR_CONTEXT(component_name, async_context, execution_result, \
                          message, ...)                                    \
  __SCP_LOG_FAIL_CONTEXT(google::scp::core::logger::LogLevel::kError,      \
                         component_name, async_context, execution_result,  \
                         message, ##__VA_ARGS__)

#define SCP_CRITICAL(component_name, activity_id, execution_result, message, \
                     ...)                                                    \
  __SCP_LOG_FAIL(google::scp::core::logger::LogLevel::kCritical,             \
                 component_name, activity_id, execution_result, message,     \
                 ##__VA_ARGS__)

#define SCP_CRITICAL_CONTEXT(component_name, async_context, execution_result, \
                             message, ...)                                    \
  __SCP_LOG_FAIL_CONTEXT(google::scp::core::logger::LogLevel::kCritical,      \
                         component_name, async_context, execution_result,     \
                         message, ##__VA_ARGS__)

#define SCP_ALERT(component_name, activity_id, execution_result, message, ...) \
  __SCP_LOG_FAIL(google::scp::core::logger::LogLevel::kAlert, component_name,  \
                 activity_id, execution_result, message, ##__VA_ARGS__)

#define SCP_ALERT_CONTEXT(component_name, async_context, execution_result, \
                          message, ...)                                    \
  __SCP_LOG_FAIL_CONTEXT(google::scp::core::logger::LogLevel::kAlert,      \
                         component_name, async_context, execution_result,  \
                         message, ##__VA_ARGS__)

#define SCP_EMERGENCY(component_name, activity_id, execution_result, message, \
                      ...)                                                    \
  __SCP_LOG_FAIL(google::scp::core::logger::LogLevel::kEmergency,             \
                 component_name, activity_id, execution_result, message,      \
                 ##__VA_ARGS__)

#define SCP_EMERGENCY_CONTEXT(component_name, async_context, execution_result, \
                              message, ...)                                    \
  __SCP_LOG_FAIL_CONTEXT(google::scp::core::logger::LogLevel::kEmergency,      \
                         component_name, async_context, execution_result,      \
                         message, ##__VA_ARGS__)

#define __SCP_LOG_FAIL(log_level, component_name, activity_id, message, ...) \
  __SCP_LOG_FAIL_IMPL(log_level, component_name,                             \
                      google::scp::core::common::kZeroUuid,                  \
                      google::scp::core::common::kZeroUuid, activity_id,     \
                      message, ##__VA_ARGS__)

#define __SCP_LOG_FAIL_CONTEXT(log_level, component_name, async_context,       \
                               execution_result, message, ...)                 \
  __SCP_LOG_FAIL_IMPL(log_level, component_name, async_context.correlation_id, \
                      async_context.parent_activity_id,                        \
                      async_context.activity_id, execution_result, message,    \
                      ##__VA_ARGS__)

#define __SCP_LOG_FAIL_IMPL(log_level, component_name, correlation_id,         \
                            parent_activity_id, activity_id, execution_result, \
                            message, ...)                                      \
  __SCP_LOG_IMPL(                                                              \
      log_level, component_name, correlation_id, parent_activity_id,           \
      activity_id,                                                             \
      absl::StrCat(                                                            \
          message, " Failed with: ",                                           \
          google::scp::core::common::internal::cpio_log::GetErrorMessage(      \
              execution_result)),                                              \
      ##__VA_ARGS__);

#define __SCP_LOG_IMPL(log_level, component_name, correlation_id,           \
                       parent_activity_id, activity_id, message, ...)       \
  if (auto* const logger =                                                  \
          google::scp::core::common::internal::cpio_log::GetLogger();       \
      logger != nullptr) {                                                  \
    logger->Log(log_level, correlation_id, parent_activity_id, activity_id, \
                component_name, SCP_LOCATION, message, ##__VA_ARGS__);      \
  }

#endif  // CORE_COMMON_GLOBAL_LOGGER_GLOBAL_LOGGER_H_
