// Copyright 2024 Google LLC
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

#include "src/util/rlimit_core_config.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

#include "rlimit_core_config.h"

namespace privacy_sandbox::server_common {
namespace {

constexpr auto CoreFileExists = []() -> bool {
  struct stat buffer;
  return stat("core", &buffer) == 0;
};

absl::Status CheckCoreFilePattern() {
  std::ifstream core_pattern_file("/proc/sys/kernel/core_pattern");
  if (!core_pattern_file.is_open()) {
    return absl::NotFoundError("Failed to open /proc/sys/kernel/core_pattern");
  }
  std::string actual_core_path;
  std::getline(core_pattern_file, actual_core_path);
  if (actual_core_path != "core") {
    return absl::FailedPreconditionError(absl::StrCat(
        "core_pattern is not set to 'core'. Actual value: ", actual_core_path));
  }
  return absl::OkStatus();
}

// Tests here have been intentionally ordered the way they are.
// Soft limits may be changed by a process to any value that is less than or
// equal to the hard limit. A process may (irreversibly) lower its hard limit to
// any value that is greater than or equal to the soft limit.
// So, if the tests were ordered differently, the hard limit would set to zero
// by either CoreDumpsAreDisabledByDefault or
// CoreDumpDisabledNoCoreFileGenerated. In that case if
// CoreDumpEnabledCoreFileGenerated were run after it, it would fail.

// Source - https://docs.oracle.com/cd/E86824_01/html/E54765/setrlimit-2.html

TEST(RlimitCoreConfigTest, CoreDumpEnabledCoreFileGenerated) {
  if (auto status = CheckCoreFilePattern(); !status.ok()) {
    GTEST_SKIP() << "Failed core_pattern check: " << status;
  }
  privacysandbox::server_common::SetRLimits({
      .enable_core_dumps = true,
  });
  std::remove("core");
  EXPECT_THAT(CoreFileExists(), ::testing::IsFalse());
  CHECK(std::system("src/util/core_dump_generator") != 0)
      << std::strerror(errno);
  EXPECT_THAT(CoreFileExists(), ::testing::IsTrue());
}

TEST(RlimitCoreConfigTest, CoreDumpsAreDisabledByDefault) {
  if (auto status = CheckCoreFilePattern(); !status.ok()) {
    GTEST_SKIP() << "Failed core_pattern check: " << status;
  }
  privacysandbox::server_common::SetRLimits();
  std::remove("core");
  EXPECT_THAT(CoreFileExists(), ::testing::IsFalse());
  CHECK(std::system("src/util/core_dump_generator") != 0)
      << std::strerror(errno);
  EXPECT_THAT(CoreFileExists(), ::testing::IsFalse());
}

TEST(RlimitCoreConfigTest, CoreDumpDisabledNoCoreFileGenerated) {
  if (auto status = CheckCoreFilePattern(); !status.ok()) {
    GTEST_SKIP() << "Failed core_pattern check: " << status;
  }
  privacysandbox::server_common::SetRLimits({
      .enable_core_dumps = false,
  });
  std::remove("core");
  EXPECT_THAT(CoreFileExists(), ::testing::IsFalse());
  CHECK(std::system("src/util/core_dump_generator") != 0)
      << std::strerror(errno);
  EXPECT_THAT(CoreFileExists(), ::testing::IsFalse());
}

}  // namespace
}  // namespace privacy_sandbox::server_common
