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

namespace privacy_sandbox::test {

namespace {
using ::testing::AllOf;
using ::testing::ContainsRegex;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

TEST_F(LogTest, NothingIfNotConsented) {
  server_common::log::AlwaysLogOtel(true);

  EXPECT_THAT(
      LogWithCapturedStderr([this]() { PS_VLOG(kMaxV, tc) << kLogContent; }),
      IsEmpty());
  EXPECT_THAT(
      LogWithCapturedStderr([this]() { PS_LOG(INFO, tc) << kLogContent; }),
      IsEmpty());
  EXPECT_THAT(
      LogWithCapturedStderr([this]() { PS_LOG(WARNING, tc) << kLogContent; }),
      IsEmpty());
  EXPECT_THAT(
      LogWithCapturedStderr([this]() { PS_LOG(ERROR, tc) << kLogContent; }),
      IsEmpty());

  tc.is_debug_response_ = true;
  EXPECT_THAT(
      LogWithCapturedStderr([this]() { PS_VLOG(kMaxV, tc) << kLogContent; }),
      IsEmpty());
  EXPECT_THAT(
      LogWithCapturedStderr([this]() { PS_LOG(INFO, tc) << kLogContent; }),
      IsEmpty());
  EXPECT_THAT(
      LogWithCapturedStderr([this]() { PS_LOG(WARNING, tc) << kLogContent; }),
      IsEmpty());
  EXPECT_THAT(
      LogWithCapturedStderr([this]() { PS_LOG(ERROR, tc) << kLogContent; }),
      IsEmpty());
}

TEST_F(LogTest, NoContext) {
  std::string log =
      LogWithCapturedStderr([]() { PS_VLOG(kMaxV) << kLogContent; });
  EXPECT_THAT(log, IsEmpty());

  log = LogWithCapturedStderr([]() { PS_LOG(INFO) << kLogContent; });
  EXPECT_THAT(log, IsEmpty());

  log = LogWithCapturedStderr([]() { PS_LOG(WARNING) << kLogContent; });
  EXPECT_THAT(log, IsEmpty());

  log = LogWithCapturedStderr([]() { PS_LOG(ERROR) << kLogContent; });
  EXPECT_THAT(log, IsEmpty());
}

}  // namespace
}  // namespace privacy_sandbox::test
