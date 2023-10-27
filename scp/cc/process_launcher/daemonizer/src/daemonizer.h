/*
 * Copyright 2022 Google LLC
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

#ifndef PROCESS_LAUNCHER_DAEMONIZER_SRC_DAEMONIZER_H_
#define PROCESS_LAUNCHER_DAEMONIZER_SRC_DAEMONIZER_H_

#include <sys/types.h>

#include <atomic>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "process_launcher/argument_parser/src/json_arg_parser.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::process_launcher {
class Daemonizer {
 public:
  Daemonizer() = delete;

  /**
   * @brief Construct a new Daemonizer object
   * The input that the daemonizer handles is expected to be in JSON format,
   * where each executable is represented with the following schema:
   * {"executable_name":"/exe/name", "command_line_args":["arg1", "arg2", ...]}
   * @param executable_count The number of executables
   * @param executables The array of executables
   */
  Daemonizer(int executable_count, char* executables[])
      : executable_count_(executable_count), executables_(executables) {}

  /**
   * @brief Launch and monitor the input processes.
   * This function blocks, and will only return on error.
   * @return google::scp::core::ExecutionResult the failure execution result.
   */
  google::scp::core::ExecutionResult Run() noexcept;

 protected:
  int executable_count_ = 0;
  char** executables_;
  JsonArgParser<ExecutableArgument> executable_arg_parser_;
  std::vector<std::shared_ptr<ExecutableArgument>> executable_args_;
  absl::flat_hash_map<pid_t, std::shared_ptr<ExecutableArgument>>
      pid_to_executable_arg_map_;
  absl::flat_hash_set<std::shared_ptr<ExecutableArgument>>
      executable_arg_to_launch_set_;

  /**
   * @brief Turn input into executable args list
   *
   */
  google::scp::core::ExecutionResult GetExecutableArgs() noexcept;

  /**
   * @brief Whether the daemonizer should stop restarting processes
   *
   * @return true or false
   */
  virtual bool ShouldStopRestartingProcesses() noexcept;
};
}  // namespace google::scp::process_launcher

#endif  // PROCESS_LAUNCHER_DAEMONIZER_SRC_DAEMONIZER_H_
