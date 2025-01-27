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

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "google/protobuf/util/json_util.h"
#include "src/communication/json_utils.h"
#include "src/roma/byob/benchmark/burst_generator.h"
#include "src/roma/byob/benchmark/roma_byob_rpc_factory.h"
#include "src/roma/byob/config/config.h"
#include "src/roma/byob/interface/roma_service.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/tools/v8_cli/roma_v8_rpc_factory.h"
#include "src/util/execution_token.h"
#include "src/util/periodic_closure.h"
#include "src/util/status_macro/status_macros.h"

ABSL_FLAG(std::string, run_id,
          google::scp::core::common::ToString(
              google::scp::core::common::Uuid::GenerateUuid()),
          "Arbitrary identifier included in the report");
ABSL_FLAG(int, num_workers, 84, "Number of pre-created workers");
ABSL_FLAG(int, queries_per_second, 42,
          "Number of queries to be sent in a second");
ABSL_FLAG(int, burst_size, 14,
          "Number of times to call ProcessRequest for a single query");
ABSL_FLAG(int, num_queries, 10'000, "Number of queries to be sent");
ABSL_FLAG(int, total_invocations, 0,
          "Number of invocations to be sent. If non-zero, overrides "
          "num_queries, and num_queries = total_invocations / burst_size.");
ABSL_FLAG(privacy_sandbox::server_common::byob::Mode, sandbox,
          privacy_sandbox::server_common::byob::Mode::kModeMinimalSandbox,
          "Run BYOB with mode: gvisor, gvisor-debug, minimal.");
ABSL_FLAG(bool, syscall_filter, false, "Whether to enable syscall filtering.");
ABSL_FLAG(std::string, lib_mounts, LIB_MOUNTS,
          "Mount paths to include in the pivot_root environment. Example "
          "/dir1,/dir2");
ABSL_FLAG(std::string, binary_path, "/udf/sample_udf", "Path to binary");
ABSL_FLAG(std::string, mode, "byob", "Traffic generator mode: 'byob' or 'v8'");
ABSL_FLAG(std::string, udf_path, "",
          "Path to JavaScript UDF file (V8 mode only)");
ABSL_FLAG(std::string, handler_name, "",
          "Name of the handler function to call (V8 mode only)");
ABSL_FLAG(std::vector<std::string>, input_args, {},
          "Arguments to pass to the handler function (V8 mode only)");
ABSL_FLAG(std::string, output_file, "", "Path to output file (JSON format)");
ABSL_FLAG(bool, verbose, false, "Enable verbose logging");
ABSL_FLAG(bool, batching, false,
          "Enable batching of BURST_SIZE queries for V8 mode");

namespace {

using ::google::scp::roma::tools::v8_cli::CreateV8RpcFunc;
using ::privacy_sandbox::server_common::PeriodicClosure;

using ExecutionFunc = absl::AnyInvocable<void(
    privacy_sandbox::server_common::Stopwatch, absl::StatusOr<absl::Duration>*,
    absl::StatusOr<std::string>*, absl::Notification*)>;
using CleanupFunc = absl::AnyInvocable<void()>;

}  // namespace

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverity::kInfo);
  const int num_workers = absl::GetFlag(FLAGS_num_workers);
  CHECK_GT(num_workers, 0);
  int burst_size = absl::GetFlag(FLAGS_burst_size);
  CHECK_GT(burst_size, 0);
  const int queries_per_second = absl::GetFlag(FLAGS_queries_per_second);
  CHECK_GT(queries_per_second, 0);
  const std::string output_file = absl::GetFlag(FLAGS_output_file);
  const bool batching = absl::GetFlag(FLAGS_batching);

  int num_queries = absl::GetFlag(FLAGS_num_queries);
  const int total_invocations = absl::GetFlag(FLAGS_total_invocations);
  CHECK_GE(total_invocations, 0);
  if (total_invocations > 0) {
    num_queries = total_invocations / burst_size;
  } else {
    CHECK_GT(num_queries, 0);
  }

  const std::string lib_mounts = absl::GetFlag(FLAGS_lib_mounts);
  const std::string binary_path = absl::GetFlag(FLAGS_binary_path);
  const privacy_sandbox::server_common::byob::Mode sandbox =
      absl::GetFlag(FLAGS_sandbox);
  const bool enable_seccomp_filter = absl::GetFlag(FLAGS_syscall_filter);

  const std::string udf_path = absl::GetFlag(FLAGS_udf_path);
  const std::string handler_name = absl::GetFlag(FLAGS_handler_name);
  const std::vector<std::string> input_args = absl::GetFlag(FLAGS_input_args);

  const std::string mode = absl::GetFlag(FLAGS_mode);
  CHECK(mode == "byob" || mode == "v8")
      << "Invalid mode. Must be 'byob' or 'v8'";

  CleanupFunc stop_func;
  ExecutionFunc rpc_func;

  const std::int64_t expected_completions = num_queries * burst_size;
  std::atomic<std::int64_t> completions = 0;

  std::unique_ptr<PeriodicClosure> periodic = PeriodicClosure::Create();
  if (absl::Status s = periodic->StartDelayed(
          absl::Seconds(1),
          [&completions, expected_completions]() {
            static int64_t previous = 0;
            const int64_t curr_val = completions;
            if (previous != expected_completions) {
              LOG(INFO) << "completions: " << curr_val
                        << ", increment: " << curr_val - previous;
            }
            previous = curr_val;
          });
      !s.ok()) {
    LOG(FATAL) << s;
  }

  if (mode == "byob") {
    std::tie(rpc_func, stop_func) =
        CreateByobRpcFunc(num_workers, lib_mounts, binary_path, sandbox,
                          completions, enable_seccomp_filter);
  } else {  // v8 mode
    std::tie(rpc_func, stop_func) =
        CreateV8RpcFunc(num_workers, udf_path, handler_name, input_args,
                        completions, burst_size, batching);
  }

  // If batching is enabled, use 1 instead of burst_size. Instead of calling
  // rpc_func with a single execution burst_size times, rpc_func is called 1
  // time with a batch of burst_size executions.
  int size = batching ? 1 : burst_size;

  using ::privacy_sandbox::server_common::byob::BurstGenerator;
  const absl::Duration burst_cadence = absl::Seconds(1) / queries_per_second;
  BurstGenerator burst_gen("tg1", num_queries, size, burst_cadence,
                           std::move(rpc_func));

  LOG(INFO) << "starting burst generator run."
            << "\n  burst size: " << burst_size
            << "\n  burst cadence: " << burst_cadence
            << "\n  num bursts: " << num_queries << std::endl;

  const BurstGenerator::Stats stats = burst_gen.Run();

  // Wait for all RPCs to complete before stopping the service
  LOG(INFO) << "Waiting for all RPCs to complete...";
  burst_gen.WaitForCompletion();
  LOG(INFO) << "All RPCs completed.";

  // RomaService must be cleaned up before stats are reported, to ensure the
  // service's work is completed
  stop_func();
  LOG(INFO) << "\n  burst size: " << burst_size
            << "\n  burst cadence: " << burst_cadence
            << "\n  num bursts: " << num_queries << std::endl;

  if (!output_file.empty()) {
    if (std::ofstream outfile(output_file, std::ios::app); outfile.is_open()) {
      auto report = stats.ToReport();
      report.set_run_id(absl::GetFlag(FLAGS_run_id));
      report.mutable_params()->set_burst_size(burst_size);
      report.mutable_params()->set_query_count(num_queries);
      report.mutable_params()->set_queries_per_second(queries_per_second);
      report.mutable_params()->set_num_workers(num_workers);

      google::protobuf::util::JsonPrintOptions options = {
          .add_whitespace = false,
          .always_print_primitive_fields = true,
      };
      auto report_json =
          privacy_sandbox::server_common::ProtoToJson(report, options);
      CHECK(report_json.ok())
          << "Failed to convert report to JSON: " << report_json.status();
      outfile << report_json.value() << std::endl;
      if (absl::GetFlag(FLAGS_verbose)) {
        LOG(INFO) << "JSON_L" << report_json.value() << "JSON_L";
      }
    } else {
      LOG(ERROR) << "Failed to open output file: " << output_file;
    }
  }

  if (absl::GetFlag(FLAGS_verbose)) {
    LOG(INFO) << stats.ToString();
  }

  return stats.late_count == 0 ? 0 : 1;
}
