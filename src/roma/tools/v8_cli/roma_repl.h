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

#ifndef ROMA_TOOLS_V8_CLI_ROMA_REPL_H_
#define ROMA_TOOLS_V8_CLI_ROMA_REPL_H_

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "src/roma/roma_service/roma_service.h"

namespace google::scp::roma::tools::v8_cli {

class RomaRepl final {
 public:
  using RomaSvc = google::scp::roma::sandbox::roma_service::RomaService<>;

  struct RomaReplOptions {
    bool enable_profilers = false;
    int num_workers = 1;
    absl::Duration execution_timeout = absl::Seconds(10);
    std::string script_filename = "";
  };

  explicit RomaRepl(RomaReplOptions options) : options_(std::move(options)) {}
  RomaRepl(const RomaRepl&) = delete;
  RomaRepl& operator=(const RomaRepl&) = delete;

  void RunShell(const std::vector<std::string>& v8_flags);

 private:
  void Load(RomaSvc* roma_service, std::string_view version_str,
            std::string_view udf_file_path);
  void Execute(RomaSvc* roma_service, absl::Span<const std::string_view> tokens,
               bool wait_for_completion, absl::Notification& execute_finished,
               std::string_view profiler_output_filename = "");
  bool ExecuteCommand(absl::Nonnull<RomaSvc*> roma_service,
                      std::string_view commands_msg, std::string_view line);
  void HandleExecute(RomaSvc* roma_service, int32_t execution_count,
                     bool execute_concurrently,
                     absl::Span<const std::string_view> tokens);

  RomaReplOptions options_;
};

}  // namespace google::scp::roma::tools::v8_cli

#endif  // ROMA_TOOLS_V8_CLI_ROMA_REPL_H_
