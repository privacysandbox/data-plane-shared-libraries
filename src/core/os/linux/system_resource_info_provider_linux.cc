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

#include "system_resource_info_provider_linux.h"

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"

#include "error_codes.h"

using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;

using google::scp::core::errors::
    SYSTEM_RESOURCE_INFO_PROVIDER_LINUX_COULD_NOT_FIND_MEMORY_INFO;
using google::scp::core::errors::
    SYSTEM_RESOURCE_INFO_PROVIDER_LINUX_COULD_NOT_OPEN_MEMINFO_FILE;
using google::scp::core::errors::
    SYSTEM_RESOURCE_INFO_PROVIDER_LINUX_COULD_NOT_PARSE_MEMINFO_LINE;

namespace {
constexpr std::string_view kMemInfoFileName = "/proc/meminfo";
constexpr std::string_view kMemInfoLineSeparator = " ";
constexpr size_t kExpectedMemInfoLinePartsCount = 3;
constexpr size_t kExpectedMemInfoLineNumericValueIndex = 1;
constexpr std::string_view kTotalAvailableMemory = "MemAvailable";
}  // namespace

namespace google::scp::core::os::linux {
ExecutionResultOr<uint64_t>
SystemResourceInfoProviderLinux::GetAvailableMemoryKb() noexcept {
  auto meminfo_file_path = GetMemInfoFilePath();
  std::ifstream meminfo_file(meminfo_file_path);
  if (meminfo_file.fail()) {
    return FailureExecutionResult(
        SYSTEM_RESOURCE_INFO_PROVIDER_LINUX_COULD_NOT_OPEN_MEMINFO_FILE);
  }
  // The contents of this file look like:
  // MemTotal:       198065040 kB
  // MemAvailable:   185711588 kB
  // ...

  std::string line = "";
  uint64_t total_available_mem_kb = 0;

  while (std::getline(meminfo_file, line)) {
    if (absl::StrContains(line, kTotalAvailableMemory)) {
      auto mem_value = GetMemInfoLineEntryKb(std::string_view(line));
      if (mem_value.Successful()) {
        total_available_mem_kb = *mem_value;
      } else {
        return mem_value.result();
      }
    }
  }

  if (total_available_mem_kb < 1) {
    return FailureExecutionResult(
        SYSTEM_RESOURCE_INFO_PROVIDER_LINUX_COULD_NOT_FIND_MEMORY_INFO);
  }

  return total_available_mem_kb;
}

std::string SystemResourceInfoProviderLinux::GetMemInfoFilePath() noexcept {
  return std::string(kMemInfoFileName);
}

ExecutionResultOr<uint64_t>
SystemResourceInfoProviderLinux::GetMemInfoLineEntryKb(
    std::string_view meminfo_line) noexcept {
  const std::vector<std::string> line_parts =
      absl::StrSplit(meminfo_line, kMemInfoLineSeparator, absl::SkipEmpty());
  if (line_parts.size() != kExpectedMemInfoLinePartsCount) {
    return FailureExecutionResult(
        SYSTEM_RESOURCE_INFO_PROVIDER_LINUX_COULD_NOT_PARSE_MEMINFO_LINE);
  }

  uint64_t read_memory_kb;
  std::stringstream int_parsing_stream;
  int_parsing_stream << line_parts.at(kExpectedMemInfoLineNumericValueIndex);
  int_parsing_stream >> read_memory_kb;
  if (!absl::SimpleAtoi(line_parts.at(kExpectedMemInfoLineNumericValueIndex),
                        &read_memory_kb)) {
    return FailureExecutionResult(
        SYSTEM_RESOURCE_INFO_PROVIDER_LINUX_COULD_NOT_PARSE_MEMINFO_LINE);
  }
  return read_memory_kb;
}
}  // namespace google::scp::core::os::linux
