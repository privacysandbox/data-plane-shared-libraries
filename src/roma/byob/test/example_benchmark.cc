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
#include <optional>
#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "src/communication/json_utils.h"
#include "src/roma/byob/test/example_roma_byob_app_service.h"

ABSL_FLAG(std::optional<std::string>, udf, std::nullopt,
          "UDF executable to be benchmarked.");
ABSL_FLAG(std::optional<std::string>, rpc, std::nullopt,
          "Name of the RPC method to invoke.");
ABSL_FLAG(std::optional<std::string>, request, std::nullopt,
          "File for the UDF request, in json format.");
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
ABSL_FLAG(absl::Duration, connection_timeout, absl::Minutes(1),
          "How long to wait for a worker to become available.");

namespace {
using privacy_sandbox::server_common::byob::example::ByobEchoService;
using privacy_sandbox::server_common::byob::example::EchoRequest;
using privacy_sandbox::server_common::byob::example::EchoResponse;

std::string LoadImpl(ByobEchoService<>& roma_service, std::string_view udf,
                     int num_workers) {
  absl::StatusOr<std::string> code_id = roma_service.Register(udf, num_workers);
  CHECK_OK(code_id);
  return *std::move(code_id);
}
void BM_Load(benchmark::State& state) {
  absl::StatusOr<ByobEchoService<>> roma_service = ByobEchoService<>::Create(
      /*config=*/
      {
          .syscall_filtering = absl::GetFlag(FLAGS_syscall_filtering),
          .disable_ipc_namespace = absl::GetFlag(FLAGS_disable_ipc_namespace),
      },
      absl::GetFlag(FLAGS_sandbox));
  CHECK_OK(roma_service);
  const std::optional<std::string> udf = absl::GetFlag(FLAGS_udf);
  CHECK(udf.has_value()) << "missing --udf flag";
  for (auto _ : state) {
    LoadImpl(*roma_service, *udf, /*num_workers=*/1);
  }
}
void EchoExecuteImpl(ByobEchoService<>& roma_service,
                     std::string_view code_token, EchoRequest request,
                     absl::Duration& connection_timeout) {
  absl::Notification notif;
  absl::Status notif_status;
  absl::StatusOr<EchoResponse> response;
  CHECK_OK(roma_service.Echo(notif, std::move(request), response,
                             /*metadata=*/{}, code_token, connection_timeout));
  CHECK(notif.WaitForNotificationWithTimeout(connection_timeout));
  CHECK_OK(notif_status);
  CHECK_OK(response);
}
void BM_Execute(benchmark::State& state) {
  absl::StatusOr<ByobEchoService<>> roma_service = ByobEchoService<>::Create(
      /*config=*/
      {
          .syscall_filtering = absl::GetFlag(FLAGS_syscall_filtering),
          .disable_ipc_namespace = absl::GetFlag(FLAGS_disable_ipc_namespace),
      },
      absl::GetFlag(FLAGS_sandbox));
  CHECK_OK(roma_service);
  const std::optional<std::string> udf = absl::GetFlag(FLAGS_udf);
  CHECK(udf.has_value()) << "missing --udf flag";
  const std::optional<std::string> rpc = absl::GetFlag(FLAGS_rpc);
  CHECK(rpc.has_value()) << "missing --rpc flag";
  const std::string json_content = [] {
    const std::optional<std::string> request = absl::GetFlag(FLAGS_request);
    CHECK(request.has_value()) << "missing --request flag";
    std::ifstream ifs(*request);
    return std::string(std::istreambuf_iterator<char>(ifs),
                       std::istreambuf_iterator<char>());
  }();
  if (*rpc == "Echo") {
    const std::string code_id =
        LoadImpl(*roma_service, *udf, /*num_workers=*/10);
    const auto request =
        ::privacy_sandbox::server_common::JsonToProto<EchoRequest>(
            json_content);
    CHECK_OK(request);
    absl::Duration connection_timeout = absl::GetFlag(FLAGS_connection_timeout);
    for (auto _ : state) {
      EchoExecuteImpl(*roma_service, code_id, *request, connection_timeout);
    }
    return;
  }
  LOG(FATAL) << "Unrecognized rpc '" << *rpc << "'";
}
}  // namespace

BENCHMARK(BM_Load);
BENCHMARK(BM_Execute);

int main(int argc, char** argv) {
  benchmark::Initialize(
      &argc, argv, +[] {
        std::cout << R"(benchmark-cli: Runs benchmarks for EchoService.

  Flags from example_benchmark.cc:
    --disable_ipc_namespace (Whether IPC namespace should be disabled.);
      default: true;
    --request (File for the UDF request, in json format.); default: ;
    --rpc (Name of the RPC method to invoke.); default: ;
    --sandbox (Sandbox mode for BYOB. Supported values: gvisor, gvisor-debug,
      minimal, nsjail.); default: nsjail;
    --syscall_filtering (Syscall filter level for BYOB. Supported values: no,
      worker-engine, untrusted.); default: untrusted;

  Flags from the Google Microbenchmarking Library:
    --benchmark_list_tests={true|false}
    --benchmark_filter=<regex>
    --benchmark_min_time=`<integer>x` OR `<float>s`
    --benchmark_min_warmup_time=<min_warmup_time>
    --benchmark_repetitions=<num_repetitions>
    --benchmark_enable_random_interleaving={true|false}
    --benchmark_report_aggregates_only={true|false}
    --benchmark_display_aggregates_only={true|false}
    --benchmark_format=<console|json|csv>
    --benchmark_out=<filename>
    --benchmark_out_format=<json|console|csv>
    --benchmark_color={auto|true|false}
    --benchmark_counters_tabular={true|false}
    --benchmark_context=<key>=<value>,...
    --benchmark_time_unit={ns|us|ms|s}
)";
      });
  absl::ParseCommandLine(argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
