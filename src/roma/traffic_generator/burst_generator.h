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

#ifndef SRC_ROMA_TRAFFIC_GENERATOR_BURST_GENERATOR_H
#define SRC_ROMA_TRAFFIC_GENERATOR_BURST_GENERATOR_H

#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/time/time.h"
#include "src/roma/traffic_generator/traffic_generator.pb.h"
#include "src/util/duration.h"

namespace google::scp::roma::traffic_generator {

class BurstGenerator final {
 public:
  struct Stats {
    explicit Stats(int burst_size, int num_bursts)
        : total_elapsed(absl::ZeroDuration()),
          total_invocation_count(0),
          total_bursts(num_bursts),
          late_count(0) {
      burst_creation_latencies.reserve(num_bursts);
      burst_processing_latencies.resize(num_bursts);
      wait_latencies.resize(burst_size * num_bursts);
      invocation_latencies.resize(burst_size * num_bursts);
      invocation_outputs.resize(burst_size * num_bursts);
    }

    Stats() : Stats(0, 0) {}

    absl::Duration total_elapsed;
    int64_t total_invocation_count;
    int total_bursts;
    int late_count;
    std::vector<absl::Duration> burst_creation_latencies;
    std::vector<absl::Duration> burst_processing_latencies;
    std::vector<absl::StatusOr<absl::Duration>> wait_latencies;
    std::vector<absl::StatusOr<absl::Duration>> invocation_latencies;
    std::vector<absl::StatusOr<std::string>> invocation_outputs;

    std::string ToString() const;
    void ToReport(::privacysandbox::apis::roma::traffic_generator::v1::Report&
                      report) const;
  };

  BurstGenerator(
      std::string id, int64_t num_bursts, int64_t burst_size,
      absl::Duration cadence,
      absl::AnyInvocable<void(
          privacy_sandbox::server_common::Stopwatch,
          absl::StatusOr<absl::Duration>*, absl::StatusOr<std::string>*,
          absl::BlockingCounter*, privacy_sandbox::server_common::Stopwatch*,
          absl::Duration*, absl::StatusOr<absl::Duration>*) const>
          func,
      absl::Duration run_duration = absl::InfiniteDuration())
      : id_(std::move(id)),
        num_bursts_(num_bursts),
        burst_size_(burst_size),
        cadence_(std::move(cadence)),
        func_(std::move(func)),
        run_duration_(run_duration) {}
  ~BurstGenerator() = default;

  Stats Run();
  absl::Duration Generate(
      std::string burst_id, absl::StatusOr<absl::Duration>* latencies_ptr,
      absl::StatusOr<std::string>* outputs_ptr, absl::BlockingCounter* counter,
      privacy_sandbox::server_common::Stopwatch* burst_stopwatch,
      absl::Duration* burst_duration,
      absl::StatusOr<absl::Duration>* wait_duration);

 private:
  std::string id_;
  int64_t num_bursts_;
  int64_t burst_size_;
  absl::Duration cadence_;
  absl::AnyInvocable<void(
      privacy_sandbox::server_common::Stopwatch,
      absl::StatusOr<absl::Duration>*, absl::StatusOr<std::string>*,
      absl::BlockingCounter*, privacy_sandbox::server_common::Stopwatch*,
      absl::Duration*, absl::StatusOr<absl::Duration>*) const>
      func_;
  absl::Duration run_duration_;
};

}  // namespace google::scp::roma::traffic_generator

#endif  // SRC_ROMA_TRAFFIC_GENERATOR_BURST_GENERATOR_H
