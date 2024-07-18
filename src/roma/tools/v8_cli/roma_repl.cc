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

#include "roma_repl.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/base/nullability.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "src/core/common/uuid/uuid.h"
#include "src/roma/interface/roma.h"
#include "src/roma/tools/v8_cli/utils.h"
#include "src/util/duration.h"

using google::scp::core::common::ToString;
using google::scp::core::common::Uuid;
using google::scp::roma::CodeObject;
using google::scp::roma::InvocationStrRequest;
using google::scp::roma::ResponseObject;
using google::scp::roma::tools::v8_cli::kTestDoublesLibraryPath;
namespace {

constexpr std::string_view kCommandsMessage =
    R"(
Shell Commands:

load - Load a User Defined Function (UDF)
Usage: load <version> <path to udf>
    Note: If PATH_TO_UDF is omitted, the UDF will be read from the command line.
Example: load v1 src/roma/tools/v8_cli/sample.js

execute - Execute a User Defined Function (UDF)
Usage: execute[:[C]count] <version> <udf name> [args...]
    Note: If count is specified, "C" can be added as a prefix to execute count times concurrently.

Example: execute v1 HandleFunc foo bar
Example: execute:5 v1 HandleFunc foo bar
Example: execute:C8 v1 HandleFunc foo bar

help - Display all shell commands
Usage: help

exit - Exit the tool
Usage: exit
)";

constexpr std::string_view kCommandsWithProfilerMessage =
    R"(
Shell Commands:

load - Load a User Defined Function (UDF)
Usage: load <version> <path to udf>
    Note: If path to udf is omitted, the UDF will be read from the command line.
Example: load v1 src/roma/tools/v8_cli/sample.js

execute - Execute a User Defined Function (UDF)
Usage: execute[:[C]count] <profiler output file> <version> <udf name> [args...]
    Note: If count is specified, "C" can be added as a prefix to execute count times concurrently.

Example: execute foo/bar/profiler_output.txt v1 HandleFunc foo bar
Example: execute:2 foo/bar/profiler_output.txt v1 HandleFunc foo bar
Example: execute:C4 foo/bar/profiler_output.txt v1 HandleFunc foo bar

help - Display all shell commands
Usage: help

exit - Exit the tool
Usage: exit
)";

// Get UDF from command line or input file if specified
std::string GetUDF(std::string_view udf_file_path) {
  std::string js;
  if (udf_file_path.empty()) {
    std::cout << "Please provide the JavaScript UDF. Press Enter to finish."
              << std::endl;
    std::string js_line;
    while (true) {
      if (!std::getline(std::cin, js_line) || js_line.empty()) {
        break;
      }
      absl::StrAppend(&js, js_line, "\n");
    }
    LOG(INFO) << js;
  } else {
    // Build Roma CodeOjbect from UDF code file.
    LOG(INFO) << "Loading UDF from file \"" << udf_file_path << "\"...";
    std::ifstream input_str(udf_file_path.data());
    std::string udf_js_code((std::istreambuf_iterator<char>(input_str)),
                            (std::istreambuf_iterator<char>()));
    js = udf_js_code;
  }
  return js;
}

std::vector<std::string> GetCommandsFromFile(const std::string& filename) {
  std::ifstream file(filename);
  std::vector<std::string> lines;

  if (file.is_open()) {
    std::string line;
    while (std::getline(file, line)) {
      if (!line.empty()) {
        lines.push_back(line);
      }
    }
    // Add exit as final command if not explicitly included
    if (!absl::StartsWith(line, "exit")) {
      lines.push_back("exit");
    }
    file.close();
  } else {
    std::cerr << "Error: Unable to open file " << filename << std::endl;
  }

  return lines;
}

void WriteProfilerOutput(std::string_view profiler_output_filename,
                         std::string_view profiler_output) {
  std::filesystem::path pathObj(profiler_output_filename);
  std::filesystem::create_directories(pathObj.parent_path());
  if (std::ofstream outfile(pathObj, std::ios::app); outfile.is_open()) {
    LOG(INFO) << "Writing profiler output to " << profiler_output_filename;
    outfile << profiler_output << std::endl;
  } else {
    std::cerr << "> Unable to write profiler output to file "
              << profiler_output_filename << std::endl;
  }
}

}  // namespace

namespace google::scp::roma::tools::v8_cli {

void RomaRepl::Load(RomaSvc* roma_service, std::string_view version_str,
                    std::string_view udf_file_path) {
  std::string js =
      absl::StrCat(GetUDF(kTestDoublesLibraryPath), GetUDF(udf_file_path));
  if (js.empty()) {
    std::cout << "Empty UDF cannot be loaded. Please try again. " << std::endl;
    return;
  }
  const CodeObject code_object = {
      .id = ToString(Uuid::GenerateUuid()),
      .version_string = std::string(version_str),
      .js = js.data(),
  };

  LOG(INFO) << "UDF JS code loaded!";
  LOG(INFO) << "CodeObject:\nid: " << code_object.id
            << "\nversion_string: " << code_object.version_string << "\njs:\n"
            << code_object.js;

  absl::Notification load_finished;
  LOG(INFO) << "Calling LoadCodeObj...";
  privacy_sandbox::server_common::Stopwatch timer;
  CHECK(
      roma_service
          ->LoadCodeObj(std::make_unique<CodeObject>(code_object),
                        [&load_finished](absl::StatusOr<ResponseObject> resp) {
                          if (resp.ok()) {
                            LOG(INFO) << "LoadCodeObj successful!";
                          } else {
                            std::cerr << "> load unsuccessful with status: "
                                      << resp.status() << std::endl;
                          }
                          load_finished.Notify();
                        })
          .ok());
  load_finished.WaitForNotification();
  std::cout << "> load duration: "
            << absl::ToDoubleMilliseconds(timer.GetElapsedTime()) << " ms"
            << std::endl;
}

void RomaRepl::Execute(RomaSvc* roma_service,
                       absl::Span<const std::string_view> toks,
                       bool wait_for_completion,
                       absl::Notification& execute_finished,
                       std::string_view profiler_output_filename) {
  std::vector<std::string> input;
  std::transform(toks.begin() + 2, toks.end(), std::back_inserter(input),
                 [](auto s) { return absl::StrCat("\"", s, "\""); });
  InvocationStrRequest<> execution_object = {
      .id = google::scp::core::common::ToString(
          google::scp::core::common::Uuid::GenerateUuid()),
      .version_string = std::string(toks[0]),
      .handler_name = std::string(toks[1]),
      .tags = {{
          "TimeoutDuration",
          absl::StrCat(absl::ToDoubleMilliseconds(options_.execution_timeout),
                       "ms"),
      }},
      .input = input,
  };
  LOG(INFO) << "ExecutionObject:\nid: " << execution_object.id
            << "\nversion_string: " << execution_object.version_string
            << "\nhandler_name: " << execution_object.handler_name
            << "\ninput: " << absl::StrJoin(input, " ");
  LOG(INFO) << "Calling Execute...";
  const bool allow_profilers =
      options_.enable_profilers && !profiler_output_filename.empty();

  privacy_sandbox::server_common::Stopwatch timer;
  CHECK(roma_service
            ->Execute(
                std::make_unique<InvocationStrRequest<>>(execution_object),
                [allow_profilers, profiler_output_filename,
                 &execute_finished](absl::StatusOr<ResponseObject> resp) {
                  if (resp.ok()) {
                    LOG(INFO) << "Execute successful!";
                    std::string result = std::move(resp->resp);
                    std::cout << "> " << result << std::endl;

                    if (allow_profilers && !resp->profiler_output.empty()) {
                      WriteProfilerOutput(profiler_output_filename,
                                          resp->profiler_output);
                    }
                  } else {
                    std::cerr << "> unsuccessful with status: " << resp.status()
                              << std::endl;
                  }
                  execute_finished.Notify();
                })
            .ok());

  if (wait_for_completion) {
    execute_finished.WaitForNotificationWithTimeout(options_.execution_timeout);
    std::cout << "> execute duration: "
              << absl::ToDoubleMilliseconds(timer.GetElapsedTime()) << " ms"
              << std::endl;
  }
}

/* Handle calling Execute, accounting for profiler support. `toks` contains
 * all std::string passed from stdin, and is structured in the following format:
 *
 * Without profilers: {"execute", <version>, <udf name>, [args...]}
 * With profilers: {"execute", <profiler output file>, <version>, <udf name>,
 * [args...]}
 */
void RomaRepl::HandleExecute(RomaSvc* roma_service, int32_t execution_count,
                             bool execute_concurrently,
                             absl::Span<const std::string_view> toks) {
  std::vector<absl::Notification> finished(execution_count);
  privacy_sandbox::server_common::Stopwatch timer;

  if (options_.enable_profilers) {
    std::string_view profiler_output_filename = toks[1];
    absl::Span args(toks.data() + 2, toks.size() - 2);
    for (auto i = 0; i < execution_count; ++i) {
      Execute(roma_service, args, !execute_concurrently, finished[i],
              profiler_output_filename);
    }
  } else {
    absl::Span args(toks.data() + 1, toks.size() - 1);
    for (auto i = 0; i < execution_count; ++i) {
      Execute(roma_service, args, !execute_concurrently, finished[i]);
    }
  }

  if (execute_concurrently) {
    for (int i = 0; i < execution_count; i++) {
      finished[i].WaitForNotificationWithTimeout(options_.execution_timeout);
    }
    std::cout << "> execute duration: "
              << absl::ToDoubleMilliseconds(timer.GetElapsedTime()) << " ms"
              << std::endl;
  }
}

// Execute a single command, returns false upon RomaService stoppage.
bool RomaRepl::ExecuteCommand(absl::Nonnull<RomaSvc*> roma_service,
                              std::string_view commands_msg,
                              std::string_view line) {
  std::vector<std::string_view> toks = absl::StrSplit(line, ' ');
  std::pair<std::string_view, std::string_view> command_toks =
      absl::StrSplit(toks[0], absl::MaxSplits(':', 1));
  std::string_view command = command_toks.first;
  int32_t command_count = 1;
  bool execute_concurrently = false;
  if (!command_toks.second.empty() && command_toks.second[0] == 'C') {
    execute_concurrently = true;
    command_toks.second.remove_prefix(1);
  }
  if (!command_toks.second.empty() &&
      !absl::SimpleAtoi(command_toks.second, &command_count)) {
    std::cout << "Warning: unable to parse execution count: ["
              << command_toks.second << "]" << std::endl;
  }

  if (command == "exit") {
    roma_service->Stop().IgnoreError();
    return false;
  }
  if (command == "load" && toks.size() > 1) {
    std::string_view udf_file_path;
    if (toks.size() > 2) {
      udf_file_path = toks[2];
    }
    Load(roma_service, toks[1], udf_file_path);
  } else if (command == "execute" && toks.size() > 2) {
    HandleExecute(roma_service, command_count, execute_concurrently, toks);
  } else if (command == "help") {
    std::cout << commands_msg << std::endl;
  } else {
    std::cout << "Warning: unknown command " << command << "." << std::endl;
    std::cout << "Try help for options." << std::endl;
  }
  return true;
}

// The read-eval-execute loop of the shell.
void RomaRepl::RunShell(const std::vector<std::string>& v8_flags) {
  RomaSvc::Config config;
  config.SetV8Flags() = v8_flags;
  LOG(INFO) << "V8 flags: "
            << (config.GetV8Flags().empty()
                    ? "<none>"
                    : absl::StrJoin(config.GetV8Flags(), " "));
  auto logging_fn = [](absl::LogSeverity severity,
                       const RomaSvc::TMetadata& metadata,
                       std::string_view msg) {
    std::cerr << "console: [" << absl::LogSeverityName(severity) << "] " << msg
              << std::endl;
  };
  config.SetLoggingFunction(std::move(logging_fn));
  config.enable_profilers = options_.enable_profilers;

  LOG(INFO) << "Roma config set to " << options_.num_workers << " workers.";
  config.number_of_workers = options_.num_workers;

  LOG(INFO) << "Initializing RomaService...";
  RomaSvc roma_service(std::move(config));
  CHECK_OK(roma_service.Init());
  LOG(INFO) << "RomaService Initialization successful.";

  std::string_view commands_msg = options_.enable_profilers
                                      ? kCommandsWithProfilerMessage
                                      : kCommandsMessage;
  std::cout << commands_msg << std::endl;

  std::vector<std::string> commands;
  if (!options_.script_filename.empty()) {
    commands = GetCommandsFromFile(options_.script_filename);
    if (!commands.empty()) {
      LOG(INFO) << "Read " << commands.size() << " commands from \""
                << options_.script_filename << "\"";
    }
  }

  for (int i = 0; commands.empty() || i < commands.size(); i++) {
    std::string line;
    if (commands.empty()) {
      std::cout << "> ";
      if (!std::getline(std::cin, line)) {
        break;
      }
    } else {
      line = commands[i];
    }

    if (!ExecuteCommand(&roma_service, commands_msg, line)) {
      break;
    }
  }
}

}  // namespace google::scp::roma::tools::v8_cli
