/*
 * Copyright 2023 Google LLC
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

// This test validates that for prod builds, the default implementation of the
// logging function is a no op.

#include <gtest/gtest.h>

#include <string_view>

#include "absl/base/log_severity.h"
#include "absl/log/log.h"
#include "absl/log/scoped_mock_log.h"
#include "src/roma/config/config.h"

namespace google::scp::roma::test {

void LoggingFunction(absl::LogSeverity severity,
                     absl::flat_hash_map<std::string, std::string> metadata,
                     std::string_view msg) {
  LOG(LEVEL(severity)) << msg;
}

void TestLogFunction(const int num_logs_expected,
                     const bool register_log_function,
                     absl::LogSeverity severity = absl::LogSeverity::kInfo) {
  Config<> config;
  if (register_log_function) {
    config.SetLoggingFunction(LoggingFunction);
  }
  const auto& logging_func = config.GetLoggingFunction();
  const std::string kLogMsg = "Hello World";

  absl::ScopedMockLog log;
  log.StartCapturingLogs();
  EXPECT_CALL(log, Log(severity, testing::_, kLogMsg)).Times(num_logs_expected);
  logging_func(severity, {}, kLogMsg);
  log.StopCapturingLogs();
}

TEST(ConfigLogTest, DefaultLoggingIsNoOp) {
  constexpr int num_logs_expected = 0;
  constexpr bool register_log_function = false;
  TestLogFunction(num_logs_expected, register_log_function);
}

TEST(ConfigLogTest, RegisteredLogFunctionIsCalled) {
  constexpr int num_logs_expected = 1;
  constexpr bool register_log_function = true;
  TestLogFunction(num_logs_expected, register_log_function);
}

}  // namespace google::scp::roma::test
