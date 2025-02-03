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

#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/byob/sample_udf/sample_roma_byob_app_service.h"

enum class SortListUdf { k10K, k100K, k1M };

ABSL_FLAG(std::optional<int>, num_workers, std::nullopt,
          "Number of pre-created workers");
ABSL_FLAG(int, load_iterations, 1, "Number of times to load binary");
ABSL_FLAG(int, execute_iterations, 1, "Number of times to execute binary");
ABSL_FLAG(SortListUdf, sort_list_udf, SortListUdf::k10K,
          "Which sort list UDF to run");
ABSL_FLAG(std::string, output, "", "Where to save memory usage");

namespace {
using ::privacy_sandbox::roma_byob::example::ByobSampleService;
using ::privacy_sandbox::roma_byob::example::SortListRequest;
using ::privacy_sandbox::roma_byob::example::SortListResponse;
using ::privacy_sandbox::server_common::byob::Mode;
}  // namespace

bool AbslParseFlag(absl::string_view text, SortListUdf* sort_list_udf,
                   std::string* error) {
  if (text == "10k") {
    *sort_list_udf = SortListUdf::k10K;
    return true;
  }
  if (text == "100k") {
    *sort_list_udf = SortListUdf::k100K;
    return true;
  }
  if (text == "1m") {
    *sort_list_udf = SortListUdf::k1M;
    return true;
  }
  *error = "unknown value for enumeration";
  return false;
}

std::string AbslUnparseFlag(SortListUdf sort_list_udf) {
  switch (sort_list_udf) {
    case SortListUdf::k10K:
      return "10k";
    case SortListUdf::k100K:
      return "100k";
    case SortListUdf::k1M:
      return "1m";
  }
}

int MemoryUsageInBytes() {
  int fd[2];
  PCHECK(::pipe(fd) != -1);
  const int pid = ::vfork();
  PCHECK(pid != -1);
  if (pid == 0) {
    ::close(fd[0]);
    PCHECK(::dup2(fd[1], STDOUT_FILENO) != -1);
    const char* argv[] = {
        "/usr/byob/gvisor/bin/runsc",
        "events",
        "-stats",
        "roma_server",
        nullptr,
    };
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve()";
  }
  ::close(fd[1]);
  PCHECK(::waitpid(pid, nullptr, /*options=*/0) == pid);
  FILE* const stream = ::fdopen(fd[0], "r");
  PCHECK(stream != nullptr);
  std::string output;
  char buffer[1024];
  while (::fgets(buffer, sizeof(buffer), stream) != nullptr) {
    absl::StrAppend(&output, buffer);
  }
  ::close(fd[0]);
  nlohmann::json event_stats;
  event_stats = nlohmann::json::parse(std::move(output));
  return event_stats["data"]["memory"]["usage"]["usage"].get<int>();
}

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  const std::optional<int> num_workers = absl::GetFlag(FLAGS_num_workers);
  CHECK(num_workers.has_value());
  CHECK_GT(*num_workers, 0);
  absl::StatusOr<ByobSampleService<>> sample_interface =
      ByobSampleService<>::Create(
          {
              .roma_container_name = "roma_server",
          },
          Mode::kModeGvisorSandbox);
  CHECK_OK(sample_interface);

  // Load UDF.
  const std::string code_token = [&sample_interface,
                                  num_workers = *num_workers] {
    absl::StatusOr<std::string> code_token;
    const SortListUdf sort_list_udf = absl::GetFlag(FLAGS_sort_list_udf);
    const int load_iterations = absl::GetFlag(FLAGS_load_iterations);
    CHECK_GT(load_iterations, 0);
    for (int i = 0; i < load_iterations; ++i) {
      switch (sort_list_udf) {
        case SortListUdf::k10K:
          code_token =
              sample_interface->Register("/udf/sort_list_10k_udf", num_workers);
          break;
        case SortListUdf::k100K:
          code_token = sample_interface->Register("/udf/sort_list_100k_udf",
                                                  num_workers);
          break;
        case SortListUdf::k1M:
          code_token =
              sample_interface->Register("/udf/sort_list_1m_udf", num_workers);
          break;
        default:
          LOG(FATAL) << "Unexpected sort_list_udf input";
      }
      CHECK_OK(code_token);
    }
    return *std::move(code_token);
  }();
  std::ofstream ofs(absl::GetFlag(FLAGS_output));
  ofs << "iteration_number,bytes\n";
  ofs << "0," << MemoryUsageInBytes() << "\n";

  // Run executions.
  SortListRequest request;
  const int execute_iterations = absl::GetFlag(FLAGS_execute_iterations);
  int iteration_number = 0;
  while (iteration_number++ < execute_iterations) {
    absl::Notification done;
    absl::StatusOr<std::unique_ptr<SortListResponse>> response;
    if (auto execution_token =
            sample_interface->SortList(done, request, response,
                                       /*metadata=*/{}, code_token);
        !execution_token.ok()) {
      LOG(ERROR) << "Execution failure: " << execution_token.status();
    }
    done.WaitForNotification();
    ofs << iteration_number << "," << MemoryUsageInBytes() << "\n";
    CHECK_OK(response);
  }
  return 0;
}
