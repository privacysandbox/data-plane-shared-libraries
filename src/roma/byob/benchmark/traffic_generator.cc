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
#include "absl/status/status.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/byob/config/config.h"
#include "src/roma/byob/interface/roma_service.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/util/execution_token.h"
#include "src/util/status_macro/status_macros.h"

ABSL_FLAG(int, num_workers, 84, "Number of pre-created workers");
ABSL_FLAG(int, queries_per_second, 42,
          "Number of queries to be sent in a second");
ABSL_FLAG(int, burst_size, 14,
          "Number of times to call ProcessRequest for a single query");
ABSL_FLAG(int, num_queries, 10'000, "Number of queries to be sent");
ABSL_FLAG(privacy_sandbox::server_common::byob::Mode, sandbox,
          privacy_sandbox::server_common::byob::Mode::kModeNoSandbox,
          "Run BYOB in sandbox mode.");

const std::filesystem::path kUdfPath = "/udf/sample_udf";
// Benchmark data with two columns. Success or not [1 or 0] and time taken to
// get there.
const std::filesystem::path kBenchmarkDataPath = "/data/benchmark.csv";

namespace {
using AppService = ::privacy_sandbox::server_common::byob::RomaService<>;
using Config = ::privacy_sandbox::server_common::byob::Config<>;
using Mode = ::privacy_sandbox::server_common::byob::Mode;
using ::privacy_sandbox::roma_byob::example::FUNCTION_HELLO_WORLD;
using ::privacy_sandbox::roma_byob::example::FUNCTION_PRIME_SIEVE;
using ::privacy_sandbox::roma_byob::example::SampleRequest;
using ::privacy_sandbox::roma_byob::example::SampleResponse;
}  // namespace

void SendBurst(AppService* roma_service, std::string_view code_token,
               int query_number, int burst_size) {
  for (int exec_count = 1; exec_count <= burst_size; exec_count++) {
    SampleRequest request;
    request.set_function(FUNCTION_HELLO_WORLD);
    int execution_number = query_number * burst_size + exec_count;
    absl::StatusOr<google::scp::roma::ExecutionToken> exec_token =
        roma_service->ProcessRequest<SampleResponse>(
            code_token, std::move(request),
            google::scp::roma::DefaultMetadata(),
            [execution_number](absl::StatusOr<SampleResponse> response) {
              std::cout << "SUCCESS :" << execution_number << std::endl
                        << std::flush;
              CHECK_OK(response);
            });
    CHECK_OK(exec_token) << "FAIL   : " << execution_number << std::endl
                         << std::flush;
  }
}

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  const int num_workers = absl::GetFlag(FLAGS_num_workers);
  CHECK_GT(num_workers, 0);
  const int num_queries = absl::GetFlag(FLAGS_num_queries);
  CHECK_GT(num_queries, 0);
  const int burst_size = absl::GetFlag(FLAGS_burst_size);
  CHECK_GT(burst_size, 0);
  const int queries_per_second = absl::GetFlag(FLAGS_queries_per_second);
  CHECK_GT(queries_per_second, 0);

  std::unique_ptr<AppService> roma_service = std::make_unique<AppService>();
  CHECK_OK(roma_service->Init(/*config=*/{}, absl::GetFlag(FLAGS_sandbox)));
  absl::StatusOr<std::string> code_token =
      roma_service->LoadBinary(kUdfPath, num_workers);
  CHECK_OK(code_token);
  // Adding a wait to make sure the workers are ready for work.
  absl::SleepFor(absl::Milliseconds(5'000));

  // The qps will be a little lower because disregards the time it takes to send
  // the burst. We can change this later by sleeping for
  // interval_between_send_bursts - time taken to send the burst.
  const absl::Duration interval_between_send_bursts =
      absl::Milliseconds(1000.00 / queries_per_second);
  for (int query = 0; query < num_queries; query++) {
    absl::SleepFor(interval_between_send_bursts);
    SendBurst(roma_service.get(), *code_token, query, burst_size);
  }
  return 0;
}
