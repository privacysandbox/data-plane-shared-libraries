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
#include "absl/flags/usage.h"
#include "absl/log/initialize.h"
#include "absl/strings/match.h"
#include "absl/time/time.h"
#include "src/roma/tools/v8_cli/roma_repl.h"
#include "src/roma/tools/v8_cli/utils.h"
#include "src/util/duration.h"

namespace {
using google::scp::roma::tools::v8_cli::ExtractAndSanitizeCustomFlags;
using google::scp::roma::tools::v8_cli::RomaRepl;

constexpr absl::Duration kRequestTimeout = absl::Seconds(10);
const std::vector<std::string> kRomaShellFlags = {
    "--num_workers", "--verbose", "--enable_profilers", "--timeout", "--file",
};

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

  std::vector<std::string> v8_flags =
      ExtractAndSanitizeCustomFlags(argc, argv, kRomaShellFlags);

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
