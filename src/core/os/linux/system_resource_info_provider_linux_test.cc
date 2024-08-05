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

#include "src/core/os/linux/system_resource_info_provider_linux.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <string_view>

using ::testing::AllOf;
using ::testing::Gt;

namespace google::scp::core::os::linux::test {
class SystemResourceInfoProviderLinuxForTests
    : public SystemResourceInfoProviderLinux {
 public:
  explicit SystemResourceInfoProviderLinuxForTests(
      std::string_view mem_info_file_path)
      : mem_info_file_path_(mem_info_file_path) {}

  std::string GetMemInfoFilePath() noexcept override {
    return mem_info_file_path_;
  }

 private:
  std::string mem_info_file_path_;
};

TEST(SystemResourceInfoProviderLinux, ShouldReadMemInfoIfValidFile) {
  SystemResourceInfoProviderLinuxForTests mem_info(
      "src/core/os/linux/files/valid_meminfo_file.txt");
  EXPECT_EQ(*mem_info.GetAvailableMemoryKb(), 7922601);
}

TEST(SystemResourceInfoProviderLinux, ShouldFailIfMemInfoFileIsInvalid) {
  SystemResourceInfoProviderLinuxForTests mem_info(
      "src/core/os/linux/files/invalid_format_meminfo_file.txt");
  EXPECT_FALSE(mem_info.GetAvailableMemoryKb().result().Successful());
}

TEST(SystemResourceInfoProviderLinux,
     ShouldFailIfExpectedFieldMissingInMemInfoFile) {
  SystemResourceInfoProviderLinuxForTests mem_info(
      "src/core/os/linux/files/missing_available_meminfo_file.txt");
  EXPECT_FALSE(mem_info.GetAvailableMemoryKb().result().Successful());
}

TEST(SystemResourceInfoProviderLinux, ShouldFailIfMemInfoFileDoesNotExist) {
  SystemResourceInfoProviderLinuxForTests mem_info(
      "s/file/that/does/not/exist.txt");
  EXPECT_FALSE(mem_info.GetAvailableMemoryKb().result().Successful());
}

TEST(SystemResourceInfoProviderLinux,
     ShouldReadActualMemInfoFileOnLinuxSystem) {
  SystemResourceInfoProviderLinux mem_info;
  ASSERT_THAT(*mem_info.GetAvailableMemoryKb(), Gt(1));
}
}  // namespace google::scp::core::os::linux::test
