/*
 * Copyright 2024 Google LLC
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
#include "utils.h"

#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/match.h"

namespace google::scp::roma::tools::v8_cli {
bool IsCustomFlag(const std::vector<std::string>& custom_flags,
                  std::string_view flag) {
  for (const auto& custom_flag : custom_flags) {
    if (absl::StartsWith(flag, custom_flag)) {
      return false;
    }
  }
  return true;
}

std::vector<std::string> ExtractAndSanitizeCustomFlags(
    int argc, char* argv[], const std::vector<std::string>& absl_flags) {
  std::vector<std::string> custom_flags;
  // Sanitized V8 flags to be passed to the RomaService.
  std::vector<std::string> sanitized_custom_flags;
  absl::flat_hash_set<std::string> processed_flags;
  for (int i = 1; i < argc; i++) {
    std::string_view flag = argv[i];
    std::string_view flag_clean = flag;
    if (const size_t eq_pos = flag_clean.find('='); eq_pos != flag_clean.npos) {
      flag_clean.remove_suffix(flag_clean.size() - eq_pos);
    }
    if (!absl::StartsWith(flag_clean, kFlagPrefix)) {
      continue;
    }
    flag_clean.remove_prefix(kFlagPrefix.size());
    if (!processed_flags.contains(flag_clean) &&
        IsCustomFlag(absl_flags, flag)) {
      processed_flags.insert(std::string(flag_clean));
      sanitized_custom_flags.push_back(std::string(flag_clean));
      custom_flags.push_back(std::string(flag));
    }
  }
  absl::SetFlag(&FLAGS_undefok, sanitized_custom_flags);
  return custom_flags;
}
}  // namespace google::scp::roma::tools::v8_cli
