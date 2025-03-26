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

ABSL_FLAG(int, num_workers, 1, "Number of workers.");
ABSL_FLAG(std::optional<std::string>, commands_file, std::nullopt,
          "Text file with a list of CLI commands to execute.");
ABSL_FLAG(privacy_sandbox::server_common::byob::Mode, sandbox,
          privacy_sandbox::server_common::byob::Mode::kModeNsJailSandbox,
          privacy_sandbox::server_common::byob::kByobSandboxModeHelpText);
ABSL_FLAG(
    ::privacy_sandbox::server_common::byob::SyscallFiltering, syscall_filtering,
    ::privacy_sandbox::server_common::byob::SyscallFiltering::
        kUntrustedCodeSyscallFiltering,
    ::privacy_sandbox::server_common::byob::kByobSyscallFilteringHelpText);
ABSL_FLAG(bool, disable_ipc_namespace, true,
          "Whether IPC namespace should be disabled.");
ABSL_FLAG(std::optional<std::string>, udf_log_file, std::nullopt,
          "Path with directory to a file in which UDF logs will be stored.");
ABSL_FLAG(absl::Duration, connection_timeout, absl::Minutes(1),
          "How long to wait for a worker to become available.");

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
  std::ofstream udf_log_stream;
  if (const std::optional<std::string> udf_log_file =
          absl::GetFlag(FLAGS_udf_log_file);
      udf_log_file.has_value()) {
    udf_log_stream.open(*udf_log_file, std::ios_base::app);
  }

  // Initialize BYOB.
  absl::StatusOr<ByobEchoService<>> echo_service = ByobEchoService<>::Create(
      /*config=*/
      {
          .syscall_filtering = absl::GetFlag(FLAGS_syscall_filtering),
          .disable_ipc_namespace = absl::GetFlag(FLAGS_disable_ipc_namespace),
      },
      absl::GetFlag(FLAGS_sandbox));
  CHECK_OK(echo_service);

  // Create load and execute RPC handlers.
  auto load_fn = [&echo_service, num_workers](
                     std::string_view udf) -> absl::StatusOr<std::string> {
    return echo_service->RegisterForLogging(udf, num_workers);
  };
  auto execute_fn =
      [&echo_service, &udf_log_stream](
          std::string_view rpc, std::string_view code_token,
          std::string_view request_json) -> absl::StatusOr<std::string> {
    if (rpc == "Echo") {
      absl::Notification done;
      absl::StatusOr<std::string> json_response;
      const std::optional<std::string> udf_log_file =
          absl::GetFlag(FLAGS_udf_log_file);
      auto request = ::privacy_sandbox::server_common::JsonToProto<EchoRequest>(
          request_json);
      if (!request.ok()) {
        return request.status();
      }
      auto callback = [&done, &json_response, &udf_log_stream](
                          absl::StatusOr<EchoResponse> response,
                          absl::StatusOr<std::string_view> logs,
                          auto /*metrics*/) {
        if (!response.ok()) {
          json_response = std::move(response).status();
        } else {
          json_response =
              ::privacy_sandbox::server_common::ProtoToJson<EchoResponse>(
                  *response);
        }
        if (udf_log_stream.is_open() && logs.ok() && !logs->empty()) {
          udf_log_stream << *logs;
        }
        done.Notify();
      };
      const auto execution_token = echo_service->Echo(
          callback, *std::move(request), /*metadata=*/{}, code_token,
          absl::GetFlag(FLAGS_connection_timeout));
      if (!execution_token.ok()) {
        return execution_token.status();
      }
      if (udf_log_stream.is_open()) {
        udf_log_stream << "Execution Token: " << execution_token->value
                       << std::endl;
      }
      done.WaitForNotification();
      return json_response;
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
