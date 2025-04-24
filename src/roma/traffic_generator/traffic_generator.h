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

#ifndef SRC_ROMA_TRAFFIC_GENERATOR_TRAFFIC_GENERATOR_H_
#define SRC_ROMA_TRAFFIC_GENERATOR_TRAFFIC_GENERATOR_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/byob/config/config.h"
#include "src/util/duration.h"

ABSL_DECLARE_FLAG(std::string, run_id);
ABSL_DECLARE_FLAG(int, num_workers);
ABSL_DECLARE_FLAG(int, queries_per_second);
ABSL_DECLARE_FLAG(int, burst_size);
ABSL_DECLARE_FLAG(int, num_queries);
ABSL_DECLARE_FLAG(int, total_invocations);
ABSL_DECLARE_FLAG(privacy_sandbox::server_common::byob::Mode, sandbox);
ABSL_DECLARE_FLAG(privacy_sandbox::server_common::byob::SyscallFiltering,
                  syscall_filtering);
ABSL_DECLARE_FLAG(bool, disable_ipc_namespace);
ABSL_DECLARE_FLAG(std::string, lib_mounts);
ABSL_DECLARE_FLAG(std::string, binary_path);
ABSL_DECLARE_FLAG(std::string, mode);
ABSL_DECLARE_FLAG(std::string, udf_path);
ABSL_DECLARE_FLAG(std::string, function_name);
ABSL_DECLARE_FLAG(std::vector<std::string>, input_args);
ABSL_DECLARE_FLAG(std::string, output_file);
ABSL_DECLARE_FLAG(bool, verbose);
ABSL_DECLARE_FLAG(std::optional<int>, sigpending);
ABSL_DECLARE_FLAG(absl::Duration, duration);
ABSL_DECLARE_FLAG(bool, find_max_qps);
ABSL_DECLARE_FLAG(std::string, qps_search_bounds);
ABSL_DECLARE_FLAG(double, late_threshold);
ABSL_DECLARE_FLAG(absl::Duration, byob_connection_timeout);
ABSL_DECLARE_FLAG(bool, find_latency_budget);
ABSL_DECLARE_FLAG(double, initial_duration_ms);
ABSL_DECLARE_FLAG(bool, use_late_bursts_for_budget);
ABSL_DECLARE_FLAG(double, latency_slope_threshold);
ABSL_DECLARE_FLAG(double, duration_step_ms);

namespace google::scp::roma::traffic_generator {

class TrafficGenerator {
 public:
  using ExecutionFunc = absl::AnyInvocable<void(
      privacy_sandbox::server_common::Stopwatch,
      absl::StatusOr<absl::Duration>*, absl::StatusOr<std::string>*,
      absl::BlockingCounter*, privacy_sandbox::server_common::Stopwatch*,
      absl::Duration*, absl::StatusOr<absl::Duration>*) const>;
  using CleanupFunc = absl::AnyInvocable<void()>;
  using RpcFuncCreator =
      absl::AnyInvocable<std::pair<ExecutionFunc, CleanupFunc>(
          std::atomic<std::int64_t>&)>;

  static absl::Status Run(
      RpcFuncCreator rpc_func_creator = GetDefaultRpcFuncCreator());

 private:
  static RpcFuncCreator GetDefaultRpcFuncCreator();
};

}  // namespace google::scp::roma::traffic_generator

#endif  // SRC_ROMA_TRAFFIC_GENERATOR_TRAFFIC_GENERATOR_H_
