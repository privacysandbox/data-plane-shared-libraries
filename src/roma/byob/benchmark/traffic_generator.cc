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
#include <memory>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/byob/benchmark/burst_generator.h"
#include "src/roma/byob/config/config.h"
#include "src/roma/byob/interface/roma_service.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/util/execution_token.h"
#include "src/util/periodic_closure.h"
#include "src/util/status_macro/status_macros.h"

ABSL_FLAG(int, num_workers, 84, "Number of pre-created workers");
ABSL_FLAG(int, queries_per_second, 42,
          "Number of queries to be sent in a second");
ABSL_FLAG(int, burst_size, 14,
          "Number of times to call ProcessRequest for a single query");
ABSL_FLAG(int, num_queries, 10'000, "Number of queries to be sent");
ABSL_FLAG(privacy_sandbox::server_common::byob::Mode, sandbox,
          privacy_sandbox::server_common::byob::Mode::kModeSandbox,
          "Run BYOB in sandbox mode.");
ABSL_FLAG(std::string, lib_mounts, LIB_MOUNTS,
          "Mount paths to include in the pivot_root environment. Example "
          "/dir1,/dir2");
ABSL_FLAG(std::string, binary_path, "/udf/sample_udf", "Path to binary");

namespace {

using AppService = ::privacy_sandbox::server_common::byob::RomaService<>;
using Config = ::privacy_sandbox::server_common::byob::Config<>;
using Mode = ::privacy_sandbox::server_common::byob::Mode;
using ::privacy_sandbox::roma_byob::example::FUNCTION_HELLO_WORLD;
using ::privacy_sandbox::roma_byob::example::FUNCTION_PRIME_SIEVE;
using ::privacy_sandbox::roma_byob::example::SampleResponse;
using ::privacy_sandbox::server_common::PeriodicClosure;

}  // namespace

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverity::kInfo);
  const int num_workers = absl::GetFlag(FLAGS_num_workers);
  CHECK_GT(num_workers, 0);
  const int num_queries = absl::GetFlag(FLAGS_num_queries);
  CHECK_GT(num_queries, 0);
  const int burst_size = absl::GetFlag(FLAGS_burst_size);
  CHECK_GT(burst_size, 0);
  const int queries_per_second = absl::GetFlag(FLAGS_queries_per_second);
  CHECK_GT(queries_per_second, 0);

  std::unique_ptr<AppService> roma_service = std::make_unique<AppService>();
  CHECK_OK(roma_service->Init(/*config=*/
                              {.lib_mounts = absl::GetFlag(FLAGS_lib_mounts)},
                              absl::GetFlag(FLAGS_sandbox)));
  absl::StatusOr<std::string> code_token =
      roma_service->LoadBinary(absl::GetFlag(FLAGS_binary_path), num_workers);
  CHECK_OK(code_token);
  // Wait to make sure the workers are ready for work.
  absl::SleepFor(absl::Seconds(5));

  ::privacy_sandbox::roma_byob::example::SampleRequest request;
  request.set_function(FUNCTION_HELLO_WORLD);

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
  const auto rpc_func = [&roma_service, code_token = *code_token, &request,
                         &completions](
                            privacy_sandbox::server_common::Stopwatch stopwatch,
                            absl::StatusOr<absl::Duration>* duration) {
    absl::StatusOr<google::scp::roma::ExecutionToken> exec_token =
        roma_service->ProcessRequest<SampleResponse>(
            code_token, request, google::scp::roma::DefaultMetadata(),
            [stopwatch = std::move(stopwatch), duration,
             &completions](absl::StatusOr<SampleResponse> response) {
              *duration = stopwatch.GetElapsedTime();
              completions++;
              CHECK_OK(response);
            });
    // CHECK_OK(exec_token) << "FAIL";
    if (!exec_token.ok()) {
      *duration = exec_token.status();
      completions++;
    }
  };
  using ::privacy_sandbox::server_common::byob::BurstGenerator;
  const absl::Duration burst_cadence = absl::Seconds(1) / queries_per_second;
  BurstGenerator burst_gen("tg1", num_queries, burst_size, burst_cadence,
                           rpc_func);
  const BurstGenerator::Stats stats = burst_gen.Run();
  // RomaService must be cleaned up before stats are reported, to ensure the
  // service's work is completed
  LOG(INFO) << "Shutting down Roma";
  privacy_sandbox::server_common::Stopwatch stopwatch;
  roma_service.reset();
  LOG(INFO) << "Roma shutdown duration: " << stopwatch.GetElapsedTime();
  LOG(INFO) << stats.ToString() << std::endl;
  return 0;
}
