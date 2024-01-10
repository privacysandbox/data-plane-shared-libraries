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

// This test validates that for nonprod builds, the default implementation of
// the logging function logs using absl::Log.

#include <gtest/gtest.h>

#include <string_view>

#include "absl/log/log.h"
#include "absl/log/scoped_mock_log.h"
#include "roma/config/src/config.h"

namespace google::scp::roma::test {

TEST(ConfigLogNonProdTest, DefaultLoggingIsSuccessful) {
  Config<> config;
  const auto& logging_func = config.GetLoggingFunction();
  constexpr std::string_view kLogMsg = "Hello World";

  absl::ScopedMockLog log;
  log.StartCapturingLogs();
  EXPECT_CALL(log,
              Log(absl::LogSeverity::kInfo, testing::_, std::string{kLogMsg}));
  logging_func(absl::LogSeverity::kInfo, {}, std::string{kLogMsg}).Times(1);
  log.StopCapturingLogs();
}

}  // namespace google::scp::roma::test
