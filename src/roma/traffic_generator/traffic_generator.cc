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

#include "src/roma/traffic_generator/traffic_generator.h"

#include <sys/utsname.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
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
#include "absl/strings/str_split.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "google/protobuf/util/json_util.h"
#include "src/communication/json_utils.h"
#include "src/roma/byob/benchmark/roma_byob_rpc_factory.h"
#include "src/roma/byob/config/config.h"
#include "src/roma/byob/interface/roma_service.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/tools/v8_cli/roma_v8_rpc_factory.h"
#include "src/roma/traffic_generator/burst_generator.h"
#include "src/roma/traffic_generator/traffic_generator.pb.h"
#include "src/util/duration.h"
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
          privacy_sandbox::server_common::byob::Mode::kModeNsJailSandbox,
          privacy_sandbox::server_common::byob::kByobSandboxModeHelpText);
ABSL_FLAG(bool, syscall_filter, true, "Whether to enable syscall filtering.");
ABSL_FLAG(bool, disable_ipc_namespace, true,
          "Whether to disable syscall filtering.");
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
ABSL_FLAG(std::optional<int>, sigpending, std::nullopt,
          "Set the pending signals rlimit");
ABSL_FLAG(absl::Duration, duration, absl::InfiniteDuration(),
          "Run traffic generator for a specified duration. If set, overrides "
          "num_queries.");
ABSL_FLAG(bool, find_max_qps, false,
          "Find maximum QPS that maintains performance under threshold");
ABSL_FLAG(std::string, qps_search_bounds, "1:10000",
          "Colon-separated lower and upper bounds for QPS search");
ABSL_FLAG(double, late_threshold, 15.0,
          "Maximum acceptable percentage of late bursts (default 15%)");
ABSL_FLAG(
    absl::Duration, byob_connection_timeout, absl::ZeroDuration(),
    "Max time to wait for a worker in 'byob' mode. Ignored in 'v8' mode.");

namespace {

using ::google::scp::roma::tools::v8_cli::CreateV8RpcFunc;
using ::google::scp::roma::traffic_generator::BurstGenerator;
using ::privacy_sandbox::server_common::PeriodicClosure;
using ::privacysandbox::apis::roma::traffic_generator::v1::Report;

constexpr int kMaxQpsSearchStep = 50;

using ExecutionFunc = absl::AnyInvocable<void(
    privacy_sandbox::server_common::Stopwatch, absl::StatusOr<absl::Duration>*,
    absl::StatusOr<std::string>*, absl::Notification*) const>;
using CleanupFunc = absl::AnyInvocable<void()>;

std::string GetKernelVersion() {
  struct utsname uname_data;
  PCHECK(::uname(&uname_data) != -1) << "uname failed";
  return uname_data.release;
}

void PopulateSystemInfo(
    ::privacysandbox::apis::roma::traffic_generator::v1::SystemInfo& info) {
  struct rlimit sigpending;
  PCHECK(::getrlimit(RLIMIT_SIGPENDING, &sigpending) != -1)
      << "getrlimit SIGPENDING";
  info.mutable_rlimit_sigpending()->set_soft(sigpending.rlim_cur);
  info.mutable_rlimit_sigpending()->set_hard(sigpending.rlim_max);
  info.set_hardware_thread_count(std::thread::hardware_concurrency());
  info.set_linux_kernel(GetKernelVersion());
}

std::string StatsToJson(const BurstGenerator::Stats& stats,
                        int queries_per_second, Report& report) {
  stats.ToReport(report);
  report.set_run_id(absl::GetFlag(FLAGS_run_id));
  report.mutable_params()->set_burst_size(absl::GetFlag(FLAGS_burst_size));
  report.mutable_params()->set_queries_per_second(queries_per_second);
  report.mutable_params()->set_num_workers(absl::GetFlag(FLAGS_num_workers));
  const google::protobuf::util::JsonPrintOptions json_opts = {
      .add_whitespace = false,
      .always_print_primitive_fields = true,
  };
  auto report_json =
      privacy_sandbox::server_common::ProtoToJson(report, json_opts);
  CHECK(report_json.ok()) << "Failed to convert report to JSON: "
                          << report_json.status();
  return report_json.value();
}

BurstGenerator::Stats RunBurstGenerator(
    int queries_per_second, std::atomic<std::int64_t>& completions) {
  const int num_queries = absl::GetFlag(FLAGS_num_queries);
  const int num_workers = absl::GetFlag(FLAGS_num_workers);
  const int burst_size = absl::GetFlag(FLAGS_burst_size);
  const std::string mode = absl::GetFlag(FLAGS_mode);
  const absl::Duration duration = absl::GetFlag(FLAGS_duration);

  const std::string lib_mounts = absl::GetFlag(FLAGS_lib_mounts);
  const std::string binary_path = absl::GetFlag(FLAGS_binary_path);
  const privacy_sandbox::server_common::byob::Mode sandbox =
      absl::GetFlag(FLAGS_sandbox);
  const bool enable_seccomp_filter = absl::GetFlag(FLAGS_syscall_filter);
  const bool disable_ipc_namespace = absl::GetFlag(FLAGS_disable_ipc_namespace);

  const std::string udf_path = absl::GetFlag(FLAGS_udf_path);
  const std::string handler_name = absl::GetFlag(FLAGS_handler_name);
  const std::vector<std::string> input_args = absl::GetFlag(FLAGS_input_args);

  const absl::Duration burst_cadence = absl::Seconds(1) / queries_per_second;
  auto [rpc_func, stop_func] =
      (mode == "byob")
          ? CreateByobRpcFunc(num_workers, lib_mounts, binary_path, sandbox,
                              completions, enable_seccomp_filter,
                              disable_ipc_namespace,
                              absl::GetFlag(FLAGS_byob_connection_timeout))
          : CreateV8RpcFunc(num_workers, udf_path, handler_name, input_args,
                            completions);

  BurstGenerator burst_gen("tg1", num_queries, burst_size, burst_cadence,
                           std::move(rpc_func), duration);

  std::string burst_gen_str = absl::StrCat(
      "\n  burst size: ", burst_size, "\n  burst cadence: ", burst_cadence);
  if (duration < absl::InfiniteDuration()) {
    burst_gen_str = absl::StrCat(burst_gen_str, "\n  duration: ", duration);
  } else {
    burst_gen_str =
        absl::StrCat(burst_gen_str, "\n  num bursts: ", num_queries);
  }
  LOG(INFO) << "starting burst generator run." << burst_gen_str << std::endl;

  const BurstGenerator::Stats stats = burst_gen.Run();
  LOG(INFO) << "All RPCs completed.";

  // RomaService must be cleaned up before stats are reported, to ensure the
  // service's work is completed
  stop_func();
  LOG(INFO) << burst_gen_str << std::endl;

  return stats;
}

std::pair<int, BurstGenerator::Stats> FindMaxQps(
    double threshold_percent, std::atomic<std::int64_t>& completions) {
  const std::string qps_search_bounds = absl::GetFlag(FLAGS_qps_search_bounds);
  std::vector<std::string> qps_bounds = absl::StrSplit(qps_search_bounds, ':');
  CHECK_EQ(qps_bounds.size(), 2) << "QPS search bounds must be in the format "
                                    "lower:upper";
  int low_qps;
  CHECK(absl::SimpleAtoi(qps_bounds[0], &low_qps));
  int high_qps;
  CHECK(absl::SimpleAtoi(qps_bounds[1], &high_qps));
  int best_qps = low_qps;

  BurstGenerator::Stats best_stats;
  // Binary search QPS Bounds
  while (high_qps - low_qps > kMaxQpsSearchStep) {
    int mid_qps = (high_qps + low_qps) / 2;

    BurstGenerator::Stats stats = RunBurstGenerator(mid_qps, completions);
    const float late_burst_pct =
        static_cast<float>(std::lround(static_cast<double>(stats.late_count) /
                                       stats.total_bursts * 1000)) /
        10;
    Report report;
    report.mutable_params()->set_late_burst_threshold(threshold_percent);
    std::string report_json = StatsToJson(stats, mid_qps, report);
    if (absl::GetFlag(FLAGS_verbose)) {
      LOG(INFO) << "JSON_L" << report_json << "JSON_L";
    }

    if (late_burst_pct <= threshold_percent) {
      best_qps = mid_qps;
      best_stats = std::move(stats);
      low_qps = mid_qps + 1;
    } else {
      high_qps = mid_qps - 1;
    }

    LOG(INFO) << "Tested QPS: " << mid_qps
              << ", Late percentage: " << late_burst_pct
              << "%, Best so far: " << best_qps;
  }
  LOG(INFO) << "Best QPS Found: " << best_qps;
  return std::make_pair(best_qps, best_stats);
}

}  // namespace

namespace google::scp::roma::traffic_generator {

absl::Status TrafficGenerator::Run() {
  LOG(INFO) << "Starting traffic generator...";
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverity::kInfo);
  const int num_workers = absl::GetFlag(FLAGS_num_workers);
  CHECK_GT(num_workers, 0);
  const int burst_size = absl::GetFlag(FLAGS_burst_size);
  CHECK_GT(burst_size, 0);
  int queries_per_second = absl::GetFlag(FLAGS_queries_per_second);
  CHECK_GT(queries_per_second, 0);
  const std::string output_file = absl::GetFlag(FLAGS_output_file);
  const int total_invocations = absl::GetFlag(FLAGS_total_invocations);
  CHECK_GE(total_invocations, 0);
  if (total_invocations > 0) {
    absl::SetFlag(&FLAGS_num_queries, total_invocations / burst_size);
  }
  const int num_queries = absl::GetFlag(FLAGS_num_queries);
  CHECK_GT(num_queries, 0);

  const std::string mode = absl::GetFlag(FLAGS_mode);
  CHECK(mode == "byob" || mode == "v8")
      << "Invalid mode. Must be 'byob' or 'v8'";

  if (absl::GetFlag(FLAGS_sigpending).has_value()) {
    const int new_sigpending_limit = absl::GetFlag(FLAGS_sigpending).value();
    struct rlimit new_sigpending = {
        .rlim_cur = rlim_t(new_sigpending_limit),
        .rlim_max = rlim_t(new_sigpending_limit),
    };
    PCHECK(::setrlimit(RLIMIT_SIGPENDING, &new_sigpending) != -1)
        << "setrlimit SIGPENDING";
  }
  Report report;
  PopulateSystemInfo(*report.mutable_info());

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

  BurstGenerator::Stats stats;
  if (absl::GetFlag(FLAGS_find_max_qps)) {
    const double threshold = absl::GetFlag(FLAGS_late_threshold);
    LOG(INFO) << "Finding maximum QPS with late burst threshold of "
              << threshold << "%";

    report.mutable_params()->set_late_burst_threshold(threshold);
    std::tie(queries_per_second, stats) = FindMaxQps(threshold, completions);
  } else {
    stats = RunBurstGenerator(queries_per_second, completions);
  }

  std::string report_json = StatsToJson(stats, queries_per_second, report);
  if (absl::GetFlag(FLAGS_verbose)) {
    LOG(INFO) << "JSON_L" << report_json << "JSON_L";
  }
  if (!output_file.empty()) {
    if (std::ofstream outfile(output_file, std::ios::app); outfile.is_open()) {
      outfile << report_json << std::endl;
    } else {
      LOG(ERROR) << "Failed to open output file: " << output_file;
    }
  }

  if (absl::GetFlag(FLAGS_verbose)) {
    LOG(INFO) << stats.ToString();
  }

  return stats.late_count == 0 ? absl::OkStatus()
                               : absl::InternalError("Non-zero late bursts");
}

}  // namespace google::scp::roma::traffic_generator
