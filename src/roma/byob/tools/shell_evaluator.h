/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/function_ref.h"
#include "absl/status/statusor.h"

namespace privacy_sandbox::server_common::byob {
class ShellEvaluator {
 public:
  enum class NextStep {
    kExit = 0,
    kError = 1,
    kContinue = 2,
  };

  explicit ShellEvaluator(
      std::string_view service_specific_message, std::vector<std::string> rpcs,
      absl::FunctionRef<absl::StatusOr<std::string>(std::string_view)> load_fn,
      absl::FunctionRef<absl::StatusOr<std::string>(
          std::string_view, std::string_view, std::string_view)>
          execute_fn);

  NextStep EvalAndPrint(std::string_view line, bool disable_commands,
                        bool print_response);

 private:
  std::string_view service_specific_message_;
  absl::FunctionRef<absl::StatusOr<std::string>(std::string_view)> load_fn_;
  absl::FunctionRef<absl::StatusOr<std::string>(
      std::string_view, std::string_view, std::string_view)>
      execute_fn_;
  absl::flat_hash_map<std::string, std::optional<std::string>> rpc_to_token_;
};
}  // namespace privacy_sandbox::server_common::byob
