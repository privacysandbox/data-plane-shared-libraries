/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LOGGER_REQUEST_CONTEXT_LOGGER_TEST_H_
#define LOGGER_REQUEST_CONTEXT_LOGGER_TEST_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "src/logger/request_context_logger.h"

namespace privacy_sandbox::test {

using ::testing::StrictMock;

class LogSinkMock : public absl::LogSink {
 public:
  MOCK_METHOD(void, Send, (const absl::LogEntry&), (override));
  MOCK_METHOD(void, Flush, (), (override));
};

class TestContext : public server_common::log::PSLogContext {
 public:
  // implement interface
  std::string_view ContextStr() const override { return context_str_; }

  bool is_consented() const override { return is_consented_; }

  absl::LogSink* ConsentedSink() override { return &consent_sink_; }

  bool is_debug_response() const override { return is_debug_response_; }

  absl::LogSink* DebugResponseSink() override { return &debug_response_sink_; }

  // data source of interface
  std::string context_str_ = " log_context_str ";
  bool is_consented_ = false;
  StrictMock<LogSinkMock> consent_sink_;
  bool is_debug_response_ = false;
  StrictMock<LogSinkMock> debug_response_sink_;
};

class LogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // initialize max verbosity = kMaxV
    server_common::log::SetGlobalPSVLogLevel(kMaxV);
  }

  std::string LogWithCapturedStderr(absl::AnyInvocable<void() &&> logging) {
    testing::internal::CaptureStderr();
    std::move(logging)();
    return testing::internal::GetCapturedStderr();
  }

  static constexpr std::string_view kLogContent = "log_content";
  static constexpr int kMaxV = 5;
  TestContext tc;
};

MATCHER_P(LogEntryHas, value, "") {
  return absl::StrContains(arg.text_message(), value);
}

MATCHER_P(LogEntrySeverity, value, "") { return arg.log_severity() == value; }

inline std::string Crash() {
  CHECK(false) << "  This should not be called";
  return "";
}

}  // namespace privacy_sandbox::test

#endif  // LOGGER_REQUEST_CONTEXT_LOGGER_TEST_H_
