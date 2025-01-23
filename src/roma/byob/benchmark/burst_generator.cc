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

#include "src/roma/byob/benchmark/burst_generator.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/time/time.h"
#include "google/protobuf/util/time_util.h"
#include "src/roma/byob/benchmark/latency_formatter.h"
#include "src/roma/byob/benchmark/traffic_generator.pb.h"
#include "src/util/duration.h"

namespace {

template <typename T>
struct Percentiles {
  size_t count;
  T min;
  T p50;
  T p90;
  T p95;
  T p99;
  T max;
};

template <typename T>
Percentiles<T> get_percentiles(std::vector<T> values) {
  std::sort(values.begin(), values.end());
  return Percentiles<T>{
      .count = values.size(),
      .min = values[0],
      .p50 = values[std::floor(0.5 * values.size())],
      .p90 = values[std::floor(0.9 * values.size())],
      .p95 = values[std::floor(0.95 * values.size())],
      .p99 = values[std::floor(0.99 * values.size())],
      .max = values[values.size() - 1],
  };
}

template <typename T>
Percentiles<T> get_status_percentiles(std::vector<absl::StatusOr<T>> values) {
  std::vector<T> inv_durations;
  inv_durations.reserve(values.size());
  int failure_count = 0;
  for (absl::StatusOr<T> l : values) {
    if (l.ok()) {
      inv_durations.push_back(*l);
    } else {
      failure_count++;
    }
  }
  if (failure_count > 0) {
    LOG(ERROR) << "failure count: " << failure_count;
  }
  return get_percentiles(inv_durations);
}

google::protobuf::Duration DurationToProto(absl::Duration duration) {
  return google::protobuf::util::TimeUtil::NanosecondsToDuration(
      absl::ToInt64Nanoseconds(duration));
}

}  // namespace

namespace privacy_sandbox::server_common::byob {

std::string BurstGenerator::Stats::ToString() const {
  Percentiles<absl::Duration> burst_ptiles = get_percentiles(burst_latencies);
  Percentiles<absl::Duration> invocation_ptiles =
      get_status_percentiles(invocation_latencies);
  std::vector<absl::Duration> output_latencies =
      LatencyFormatter::Parse(invocation_outputs);

  const double late_burst_pct =
      static_cast<double>(
          std::lround(static_cast<double>(late_count) / total_bursts * 1000)) /
      10;
  auto stats_str = absl::StrCat(
      "total runtime: ", total_elapsed,
      "\n invocation count: ", total_invocation_count,
      "\n late bursts: ", late_count, " (", late_burst_pct, "%)",
      "\nburst latencies", "\n  count: ", burst_ptiles.count,
      "\n  min: ", burst_ptiles.min, "\n  p50: ", burst_ptiles.p50,
      "\n  p90: ", burst_ptiles.p90, "\n  p95: ", burst_ptiles.p95,
      "\n  p99: ", burst_ptiles.p99, "\n  max: ", burst_ptiles.max,
      "\ninvocation latencies", "\n  count: ", invocation_ptiles.count,
      "\n  min: ", invocation_ptiles.min, "\n  p50: ", invocation_ptiles.p50,
      "\n  p90: ", invocation_ptiles.p90, "\n  p95: ", invocation_ptiles.p95,
      "\n  p99: ", invocation_ptiles.p99, "\n  max: ", invocation_ptiles.max);
  // Each invocation should have a corresponding output, except in the case of
  // batch_execute, where each invocation would be associated with a batch of
  // outputs.
  if (output_latencies.size() >= invocation_outputs.size()) {
    Percentiles<absl::Duration> output_ptiles =
        get_percentiles(output_latencies);
    stats_str = absl::StrCat(
        stats_str, "\noutput latencies", "\n  count: ", output_ptiles.count,
        "\n  min: ", output_ptiles.min, "\n  p50: ", output_ptiles.p50,
        "\n  p90: ", output_ptiles.p90, "\n  p95: ", output_ptiles.p95,
        "\n  p99: ", output_ptiles.p99, "\n  max: ", output_ptiles.max);
  }
  return stats_str;
}

Report BurstGenerator::Stats::ToReport() const {
  using ::privacysandbox::apis::roma::benchmark::traffic_generator::v1::
      DurationStatistics;
  using ::privacysandbox::apis::roma::benchmark::traffic_generator::v1::Params;

  Percentiles<absl::Duration> burst_ptiles = get_percentiles(burst_latencies);
  Percentiles<absl::Duration> invocation_ptiles =
      get_status_percentiles(invocation_latencies);
  std::vector<absl::Duration> output_latencies =
      LatencyFormatter::Parse(invocation_outputs);

  Report report;

  const double late_burst_pct =
      static_cast<double>(
          std::lround(static_cast<double>(late_count) / total_bursts * 1000)) /
      10;

  auto* stats = report.mutable_statistics();
  *stats->mutable_total_elapsed_time() = DurationToProto(total_elapsed);
  stats->set_total_invocation_count(total_invocation_count);
  stats->set_late_count(late_count);
  stats->set_late_burst_pct(late_burst_pct);

  // Set burst latencies
  auto* burst_stats = report.mutable_burst_latencies();
  burst_stats->set_count(burst_ptiles.count);
  *burst_stats->mutable_min() = DurationToProto(burst_ptiles.min);
  *burst_stats->mutable_p50() = DurationToProto(burst_ptiles.p50);
  *burst_stats->mutable_p90() = DurationToProto(burst_ptiles.p90);
  *burst_stats->mutable_p95() = DurationToProto(burst_ptiles.p95);
  *burst_stats->mutable_p99() = DurationToProto(burst_ptiles.p99);
  *burst_stats->mutable_max() = DurationToProto(burst_ptiles.max);

  // Set invocation latencies
  auto* inv_stats = report.mutable_invocation_latencies();
  inv_stats->set_count(invocation_ptiles.count);
  *inv_stats->mutable_min() = DurationToProto(invocation_ptiles.min);
  *inv_stats->mutable_p50() = DurationToProto(invocation_ptiles.p50);
  *inv_stats->mutable_p90() = DurationToProto(invocation_ptiles.p90);
  *inv_stats->mutable_p95() = DurationToProto(invocation_ptiles.p95);
  *inv_stats->mutable_p99() = DurationToProto(invocation_ptiles.p99);
  *inv_stats->mutable_max() = DurationToProto(invocation_ptiles.max);

  // Each invocation should have a corresponding output, except in the case of
  // batch_execute, where each invocation would be associated with a batch of
  // outputs.
  if (output_latencies.size() >= invocation_outputs.size()) {
    Percentiles<absl::Duration> output_ptiles =
        get_percentiles(output_latencies);
    auto* output_stats = report.mutable_output_latencies();
    output_stats->set_count(output_ptiles.count);
    *output_stats->mutable_min() = DurationToProto(output_ptiles.min);
    *output_stats->mutable_p50() = DurationToProto(output_ptiles.p50);
    *output_stats->mutable_p90() = DurationToProto(output_ptiles.p90);
    *output_stats->mutable_p95() = DurationToProto(output_ptiles.p95);
    *output_stats->mutable_p99() = DurationToProto(output_ptiles.p99);
    *output_stats->mutable_max() = DurationToProto(output_ptiles.max);
  }
  return report;
}

BurstGenerator::Stats BurstGenerator::Run() {
  Stats stats(burst_size_, num_bursts_);
  privacy_sandbox::server_common::Stopwatch stopwatch;
  absl::Time expected_start = absl::Now();
  auto latencies_it = stats.invocation_latencies.begin();
  auto outputs_it = stats.invocation_outputs.begin();
  for (int i = 0; i < num_bursts_; i++) {
    std::string id = absl::StrCat("b", i);
    absl::Duration wait_time = expected_start - absl::Now();
    if (wait_time < absl::ZeroDuration()) {
      if (i > 0) {
        stats.late_count++;
      }
    } else {
      absl::SleepFor(wait_time);
    }
    absl::Duration gen_latency = Generate(id, &*latencies_it, &*outputs_it);
    stats.burst_latencies.push_back(gen_latency);
    stats.total_invocation_count += burst_size_;
    latencies_it += burst_size_;
    outputs_it += burst_size_;
    expected_start += cadence_;
  }
  stats.total_elapsed = stopwatch.GetElapsedTime();
  return stats;
}

absl::Duration BurstGenerator::Generate(
    std::string burst_id, absl::StatusOr<absl::Duration>* latencies_ptr,
    absl::StatusOr<std::string>* outputs_ptr) {
  privacy_sandbox::server_common::Stopwatch stopwatch;
  const std::string qid = absl::StrCat(id_, "-", burst_id);
  for (int i = 0; i < burst_size_; ++i) {
    privacy_sandbox::server_common::Stopwatch fn_stopwatch;
    absl::Notification* notify =
        notifications_.emplace_back(std::make_unique<absl::Notification>())
            .get();
    func_(std::move(fn_stopwatch), latencies_ptr++, outputs_ptr++, notify);
  }
  return stopwatch.GetElapsedTime();
}

}  // namespace privacy_sandbox::server_common::byob
