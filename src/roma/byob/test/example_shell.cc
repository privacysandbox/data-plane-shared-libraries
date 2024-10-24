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

#include <fstream>
#include <iostream>
#include <istream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "src/communication/json_utils.h"
#include "src/roma/byob/test/example_roma_byob_app_service.h"
#include "src/roma/byob/tools/shell_evaluator.h"

ABSL_FLAG(int, num_workers, 0, "the number of workers");
ABSL_FLAG(std::optional<std::string>, commands_file, std::nullopt,
          "a text file with a list of CLI commands to execute");
ABSL_FLAG(bool, sandbox, true, "run BYOB in sandbox mode");

using privacy_sandbox::server_common::byob::Mode;
using privacy_sandbox::server_common::byob::ShellEvaluator;
using privacy_sandbox::server_common::byob::example::ByobEchoService;
using privacy_sandbox::server_common::byob::example::EchoRequest;
using privacy_sandbox::server_common::byob::example::EchoResponse;

int main(int argc, char** argv) {
  absl::SetProgramUsageMessage("Opens an EchoService shell.");
  absl::ParseCommandLine(argc, argv);
  const int num_workers = absl::GetFlag(FLAGS_num_workers);
  CHECK_GT(num_workers, 0) << "`num_workers` must be positive";

  // Initialize BYOB.
  absl::StatusOr<ByobEchoService<>> echo_service = ByobEchoService<>::Create(
      {.num_workers = num_workers},
      absl::GetFlag(FLAGS_sandbox) ? Mode::kModeSandbox : Mode::kModeNoSandbox);
  CHECK_OK(echo_service);

  // Create load and execute RPC handlers.
  auto load_fn =
      [&echo_service](std::string_view udf) -> absl::StatusOr<std::string> {
    absl::Notification done;
    absl::Status status;
    absl::StatusOr<std::string> code_token =
        echo_service->Register(udf, done, status);
    if (!status.ok()) {
      return status;
    }
    done.WaitForNotification();
    return code_token;
  };
  auto execute_fn =
      [&echo_service](
          std::string_view rpc, std::string_view code_token,
          std::string_view request_json) -> absl::StatusOr<std::string> {
    if (rpc == "Echo") {
      absl::Notification done;
      const auto request =
          ::privacy_sandbox::server_common::JsonToProto<EchoRequest>(
              request_json);
      if (!request.ok()) {
        return request.status();
      }
      absl::StatusOr<std::unique_ptr<EchoResponse>> response;
      if (const auto execution_token = echo_service->Echo(
              done, *request, response, /*metadata=*/{}, code_token);
          !execution_token.ok()) {
        return execution_token.status();
      }
      done.WaitForNotification();
      if (!response.ok()) {
        return response.status();
      }
      return ::privacy_sandbox::server_common::ProtoToJson<EchoResponse>(
          **response);
    }
    return absl::InternalError(absl::StrCat("Unrecognized rpc '", rpc, "'"));
  };

  // Start repl.
  constexpr std::string_view kServiceSpecificMessage = R"(
load - Load a User Defined Function (UDF)
Usage: load <rpc_command> <udf_file>
Example: load Echo example_udf

Echo -
Usage: Echo <request_file> [response_file]
)";
  ShellEvaluator evaluator(kServiceSpecificMessage, {"Echo"}, load_fn,
                           execute_fn);
  if (const std::optional<std::string> commands_file =
          absl::GetFlag(FLAGS_commands_file);
      commands_file.has_value()) {
    std::ifstream ifs(*commands_file);
    if (!ifs.is_open()) {
      std::cerr << "Failed to open '" << *commands_file << "'\n";
      return -1;
    }
    std::string line;
    while (std::getline(ifs, line)) {
      switch (evaluator.EvalAndPrint(line, /*disable_commands=*/true,
                                     /*print_response=*/true)) {
        case ShellEvaluator::NextStep::kExit:
          return 0;
        case ShellEvaluator::NextStep::kError:
          return -1;
        case ShellEvaluator::NextStep::kContinue:
          continue;
      }
    }
  } else {
    std::string line;
    std::cout << "> ";
    while (std::getline(std::cin, line)) {
      switch (evaluator.EvalAndPrint(line, /*disable_commands=*/false,
                                     /*print_response=*/true)) {
        case ShellEvaluator::NextStep::kExit:
          return 0;
        case ShellEvaluator::NextStep::kError:
        case ShellEvaluator::NextStep::kContinue:
          std::cout << "> ";
          continue;
      }
    }
  }
  return 0;
}
