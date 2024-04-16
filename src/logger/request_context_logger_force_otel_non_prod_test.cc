// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/log/check.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "src/logger/request_context_logger.h"
#include "src/logger/request_context_logger_test.h"

namespace privacy_sandbox::server_common::log {

namespace {

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

TEST_F(LogTest, VlogToStderrAndOtel) {
  AlwaysLogOtel(true);

  EXPECT_CALL(
      tc.consent_sink_,
      Send(AllOf(LogEntryHas(absl::StrCat(tc.context_str_, kLogContent)),
                 LogEntrySeverity(::absl::LogSeverity::kInfo))))
      .Times(1);

  EXPECT_THAT(
      LogWithCapturedStderr([this]() { PS_VLOG(kMaxV, tc) << kLogContent; }),
      HasSubstr(absl::StrCat(tc.context_str_, kLogContent)));
}

TEST_F(LogTest, InfoToStderrAndOtel) {
  AlwaysLogOtel(true);

  EXPECT_CALL(
      tc.consent_sink_,
      Send(AllOf(LogEntryHas(absl::StrCat(tc.context_str_, kLogContent)),
                 LogEntrySeverity(::absl::LogSeverity::kInfo))))
      .Times(1);

  EXPECT_THAT(
      LogWithCapturedStderr([this]() { PS_LOG(INFO, tc) << kLogContent; }),
      HasSubstr(absl::StrCat(tc.context_str_, kLogContent)));
}

TEST_F(LogTest, WarningToStderrAndOtel) {
  AlwaysLogOtel(true);

  EXPECT_CALL(
      tc.consent_sink_,
      Send(AllOf(LogEntryHas(absl::StrCat(tc.context_str_, kLogContent)),
                 LogEntrySeverity(::absl::LogSeverity::kWarning))))
      .Times(1);

  EXPECT_THAT(
      LogWithCapturedStderr([this]() { PS_LOG(WARNING, tc) << kLogContent; }),
      HasSubstr(absl::StrCat(tc.context_str_, kLogContent)));
}

TEST_F(LogTest, ErrorToStderrAndOtel) {
  AlwaysLogOtel(true);

  EXPECT_CALL(
      tc.consent_sink_,
      Send(AllOf(LogEntryHas(absl::StrCat(tc.context_str_, kLogContent)),
                 LogEntrySeverity(::absl::LogSeverity::kError))))
      .Times(1);

  EXPECT_THAT(
      LogWithCapturedStderr([this]() { PS_LOG(ERROR, tc) << kLogContent; }),
      HasSubstr(absl::StrCat(tc.context_str_, kLogContent)));
}

}  // namespace
}  // namespace privacy_sandbox::server_common::log