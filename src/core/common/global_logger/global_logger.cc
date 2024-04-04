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

#include "global_logger.h"

#include <memory>
#include <utility>

#include "absl/base/no_destructor.h"
#include "src/core/logger/interface/log_provider_interface.h"
#include "src/core/logger/log_providers/console_log_provider.h"
#include "src/core/logger/log_providers/syslog/syslog_log_provider.h"
#include "src/core/logger/mock/mock_log_provider.h"
#include "src/public/core/interface/execution_result.h"

using google::scp::core::logger::ConsoleLogProvider;
using google::scp::core::logger::LogProviderInterface;
using google::scp::core::logger::log_providers::SyslogLogProvider;
using google::scp::core::logger::mock::MockLogProvider;

namespace google::scp::core::common {
static LogOption log_option = LogOption::kNoLog;

void InitializeCpioLog(LogOption option) { log_option = option; }

namespace internal::cpio_log {
::absl::Nullable<LogProviderInterface*> GetLogger() {
  switch (log_option) {
    case LogOption::kMock: {
      static absl::NoDestructor<MockLogProvider> provider;
      return &*provider;
    }
    case LogOption::kConsoleLog: {
      static absl::NoDestructor<ConsoleLogProvider> provider;
      return &*provider;
    }
    case LogOption::kSysLog: {
      static SyslogLogProvider provider;
      return &provider;
    }
    default:
      return nullptr;
  }
}
std::string_view GetErrorMessage(const absl::Status& status) {
  return status.message();
}

std::string_view GetErrorMessage(const ExecutionResult& result) {
  return errors::GetErrorMessage(result.status_code);
}
}  // namespace internal::cpio_log
}  // namespace google::scp::core::common
