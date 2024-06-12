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

#include <fstream>
#include <iostream>
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "src/roma/tools/v8_cli/roma_repl.h"
#include "src/util/duration.h"

namespace {
using google::scp::roma::tools::v8_cli::RomaRepl;

constexpr absl::Duration kRequestTimeout = absl::Seconds(10);
constexpr std::string_view kFlagPrefix = "--";
constexpr std::string_view kRomaShellFlags[] = {
    "--num_workers", "--verbose", "--enable_profilers", "--timeout", "--file",
};

bool IsV8Flag(std::string_view flag) {
  for (const auto& roma_flag : kRomaShellFlags) {
    if (absl::StartsWith(flag, roma_flag)) {
      return false;
    }
  }
  return true;
}
}  // namespace

ABSL_FLAG(uint16_t, num_workers, 1, "Number of Roma workers");
ABSL_FLAG(bool, verbose, false, "Log all messages from shell");
ABSL_FLAG(bool, enable_profilers, false, "Enable V8 CPU and Heap Profilers");
ABSL_FLAG(std::string, file, "",
          "Read a list of Roma CLI tool commands from the specified file");
ABSL_FLAG(absl::Duration, timeout, kRequestTimeout,
          "Pass custom timeout duration to execute commands");

int main(int argc, char* argv[]) {
  // Initialize ABSL.
  absl::InitializeLog();
  absl::SetProgramUsageMessage(
      "Opens a shell to allow for basic usage of the RomaService client to "
      "load and execute UDFs. V8 flags are recognized. V8 flags with an "
      "associated value must be specified using the form --flag=value eg. "
      "--initial-heap-size=50.");

  std::vector<std::string> v8_flags;
  // Sanitized V8 flags to be passed to the RomaService.
  std::vector<std::string> sanitized_v8_flags;
  absl::flat_hash_set<std::string> processed_flags;
  for (int i = 1; i < argc; i++) {
    std::string_view flag = argv[i];
    std::string_view flag_clean = flag;
    if (const size_t eq_pos = flag_clean.find('='); eq_pos != flag_clean.npos) {
      flag_clean.remove_suffix(flag_clean.size() - eq_pos);
    }
    flag_clean.remove_prefix(kFlagPrefix.size());
    if (!processed_flags.contains(flag_clean) && IsV8Flag(flag)) {
      processed_flags.insert(std::string(flag_clean));
      sanitized_v8_flags.push_back(std::string(flag_clean));
      v8_flags.push_back(std::string(flag));
    }
  }
  absl::SetFlag(&FLAGS_undefok, sanitized_v8_flags);

  absl::ParseCommandLine(argc, argv);
  absl::SetStderrThreshold(absl::GetFlag(FLAGS_verbose)
                               ? absl::LogSeverity::kInfo
                               : absl::LogSeverity::kWarning);
  RomaRepl repl({
      .enable_profilers = absl::GetFlag(FLAGS_enable_profilers),
      .num_workers = absl::GetFlag(FLAGS_num_workers),
      .execution_timeout = absl::GetFlag(FLAGS_timeout),
      .script_filename = absl::GetFlag(FLAGS_file),
  });
  repl.RunShell(v8_flags);

  return 0;
}
