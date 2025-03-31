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

#include "src/roma/traffic_generator/burst_generator.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/time/time.h"
#include "google/protobuf/util/time_util.h"
#include "src/roma/traffic_generator/traffic_generator.pb.h"
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
  T mad_variance;
};

template <typename T>
T get_mad_variance(std::vector<T> values) {
  T median = values[std::floor(0.5 * values.size())];
  std::vector<T> abs_deviations;
  abs_deviations.reserve(values.size());
  for (const auto& val : values) {
    if constexpr (std::is_same_v<T, absl::Duration>) {
      abs_deviations.push_back(absl::AbsDuration(val - median));
    } else {
      abs_deviations.push_back(std::abs(val - median));
    }
  }

  std::sort(abs_deviations.begin(), abs_deviations.end());
  T median_abs_deviation =
      abs_deviations[std::floor(0.5 * abs_deviations.size())];
  return median_abs_deviation;
}

template <typename T>
Percentiles<T> get_percentiles(std::vector<T> values) {
  if (values.empty()) {
    return Percentiles<T>{
        .count = 0,
        .min = T(),
        .p50 = T(),
        .p90 = T(),
        .p95 = T(),
        .p99 = T(),
        .max = T(),
        .mad_variance = T(),
    };
  }

  std::sort(values.begin(), values.end());
  return Percentiles<T>{
      .count = values.size(),
      .min = values[0],
      .p50 = values[std::floor(0.5 * values.size())],
      .p90 = values[std::floor(0.9 * values.size())],
      .p95 = values[std::floor(0.95 * values.size())],
      .p99 = values[std::floor(0.99 * values.size())],
      .max = values[values.size() - 1],
      .mad_variance = get_mad_variance(values),
  };
}

template <typename T>
std::pair<Percentiles<T>, int> get_status_percentiles(
    std::vector<absl::StatusOr<T>> values) {
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
  return std::make_pair(get_percentiles(inv_durations), failure_count);
}

google::protobuf::Duration DurationToProto(absl::Duration duration) {
  return google::protobuf::util::TimeUtil::NanosecondsToDuration(
      absl::ToInt64Nanoseconds(duration));
}

std::vector<absl::Duration> ParseOutputLatencies(
    const std::vector<absl::StatusOr<std::string>>& invocation_outputs) {
  std::vector<absl::Duration> output_latencies;
  for (const auto& output : invocation_outputs) {
    if (output.ok()) {
      absl::Duration duration;
      if (absl::ParseDuration(*output, &duration)) {
        output_latencies.push_back(duration);
      } else {
        break;
      }
    }
  }
  return output_latencies;
}

template <typename Ptile>
void CopyStats(
    const Ptile ptile,
    ::privacysandbox::apis::roma::traffic_generator::v1::DurationStatistics&
        stats) {
  stats.set_count(ptile.count);
  *stats.mutable_min() = DurationToProto(ptile.min);
  *stats.mutable_p50() = DurationToProto(ptile.p50);
  *stats.mutable_p90() = DurationToProto(ptile.p90);
  *stats.mutable_p95() = DurationToProto(ptile.p95);
  *stats.mutable_p99() = DurationToProto(ptile.p99);
  *stats.mutable_max() = DurationToProto(ptile.max);
  *stats.mutable_mad_variance() = DurationToProto(ptile.mad_variance);
}

}  // namespace

namespace google::scp::roma::traffic_generator {

std::string BurstGenerator::Stats::ToString() const {
  Percentiles<absl::Duration> burst_creation_ptiles =
      get_percentiles(burst_creation_latencies);
  Percentiles<absl::Duration> burst_processing_ptiles =
      get_percentiles(burst_processing_latencies);
  const auto [invocation_ptiles, failure_count] =
      get_status_percentiles(invocation_latencies);
  const auto wait_ptiles = get_status_percentiles(wait_latencies).first;
  std::vector<absl::Duration> output_latencies =
      ParseOutputLatencies(invocation_outputs);

  const float late_burst_pct =
      late_count > 0
          ? static_cast<float>(std::lround(static_cast<double>(late_count) /
                                           total_bursts * 1000)) /
                10
          : 0;
  const float failure_pct =
      failure_count > 0
          ? static_cast<float>(std::lround(static_cast<double>(failure_count) /
                                           total_invocation_count * 1000)) /
                10
          : 0;
  auto stats_str = absl::StrCat(
      "total runtime: ", total_elapsed,
      "\n invocation count: ", total_invocation_count,
      "\n failures: ", failure_count, " (", failure_pct, "%)",
      "\n late bursts: ", late_count, " (", late_burst_pct, "%)",
      "\nburst creation latencies", "\n  count: ", burst_creation_ptiles.count,
      "\n  min: ", burst_creation_ptiles.min,
      "\n  p50: ", burst_creation_ptiles.p50,
      "\n  p90: ", burst_creation_ptiles.p90,
      "\n  p95: ", burst_creation_ptiles.p95,
      "\n  p99: ", burst_creation_ptiles.p99,
      "\n  max: ", burst_creation_ptiles.max,
      "\n  variance (MAD): ", burst_creation_ptiles.mad_variance,
      "\nburst processing latencies",
      "\n  count: ", burst_processing_ptiles.count,
      "\n  min: ", burst_processing_ptiles.min,
      "\n  p50: ", burst_processing_ptiles.p50,
      "\n  p90: ", burst_processing_ptiles.p90,
      "\n  p95: ", burst_processing_ptiles.p95,
      "\n  p99: ", burst_processing_ptiles.p99,
      "\n  max: ", burst_processing_ptiles.max,
      "\n  variance (MAD): ", burst_processing_ptiles.mad_variance,
      "\ninvocation latencies", "\n  count: ", invocation_ptiles.count,
      "\n  min: ", invocation_ptiles.min, "\n  p50: ", invocation_ptiles.p50,
      "\n  p90: ", invocation_ptiles.p90, "\n  p95: ", invocation_ptiles.p95,
      "\n  p99: ", invocation_ptiles.p99, "\n  max: ", invocation_ptiles.max,
      "\n  variance (MAD): ", invocation_ptiles.mad_variance,
      "\nwait latencies\n  count: ", wait_ptiles.count,
      "\n  min: ", wait_ptiles.min, "\n  p50: ", wait_ptiles.p50,
      "\n  p90: ", wait_ptiles.p90, "\n  p95: ", wait_ptiles.p95,
      "\n  p99: ", wait_ptiles.p99, "\n  max: ", wait_ptiles.max,
      "\n  variance (MAD): ", wait_ptiles.mad_variance);
  // Each invocation should have a corresponding output
  if (output_latencies.size() == invocation_outputs.size()) {
    Percentiles<absl::Duration> output_ptiles =
        get_percentiles(output_latencies);
    stats_str = absl::StrCat(
        stats_str, "\noutput latencies", "\n  count: ", output_ptiles.count,
        "\n  min: ", output_ptiles.min, "\n  p50: ", output_ptiles.p50,
        "\n  p90: ", output_ptiles.p90, "\n  p95: ", output_ptiles.p95,
        "\n  p99: ", output_ptiles.p99, "\n  max: ", output_ptiles.max,
        "\n  variance (MAD): ", output_ptiles.mad_variance);
  }
  return stats_str;
}

void BurstGenerator::Stats::ToReport(
    ::privacysandbox::apis::roma::traffic_generator::v1::Report& report) const {
  using ::privacysandbox::apis::roma::traffic_generator::v1::DurationStatistics;
  using ::privacysandbox::apis::roma::traffic_generator::v1::Params;

  Percentiles<absl::Duration> burst_creation_ptiles =
      get_percentiles(burst_creation_latencies);
  Percentiles<absl::Duration> burst_processing_ptiles =
      get_percentiles(burst_processing_latencies);
  const auto [invocation_ptiles, failure_count] =
      get_status_percentiles(invocation_latencies);
  const auto wait_ptiles = get_status_percentiles(wait_latencies).first;
  std::vector<absl::Duration> output_latencies =
      ParseOutputLatencies(invocation_outputs);

  const float late_burst_pct =
      static_cast<float>(
          std::lround(static_cast<double>(late_count) / total_bursts * 1000)) /
      10;
  const float failure_pct =
      static_cast<float>(std::lround(static_cast<double>(failure_count) /
                                     total_invocation_count * 1000)) /
      10;

  auto* stats = report.mutable_statistics();
  *stats->mutable_total_elapsed_time() = DurationToProto(total_elapsed);
  stats->set_total_invocation_count(total_invocation_count);
  stats->set_late_count(late_count);
  stats->set_late_burst_pct(late_burst_pct);
  stats->set_failure_count(failure_count);
  stats->set_failure_pct(failure_pct);

  // Set query count to be the number of bursts actually executed. Will be
  // FLAGS_num_queries unless all bursts weren't able to be created within a
  // specified duration.
  report.mutable_params()->set_query_count(burst_creation_latencies.size());

  // Set burst latencies
  CopyStats(burst_creation_ptiles, *report.mutable_burst_creation_latencies());
  CopyStats(burst_processing_ptiles,
            *report.mutable_burst_processing_latencies());

  // Set invocation latencies
  CopyStats(invocation_ptiles, *report.mutable_invocation_latencies());
  CopyStats(wait_ptiles, *report.mutable_wait_latencies());

  // Each invocation should have a corresponding output
  if (output_latencies.size() == invocation_outputs.size()) {
    Percentiles<absl::Duration> output_ptiles =
        get_percentiles(output_latencies);
    CopyStats(output_ptiles, *report.mutable_output_latencies());
  }
}

BurstGenerator::Stats BurstGenerator::Run() {
  // Add a buffer to the end time to account for the last bursts
  absl::Time end_time = absl::Now() + run_duration_ + cadence_;

  Stats stats(burst_size_, num_bursts_);

  std::vector<std::unique_ptr<absl::BlockingCounter>> blocking_counters;
  blocking_counters.reserve(num_bursts_);
  for (int i = 0; i < num_bursts_; i++) {
    blocking_counters.emplace_back(
        std::make_unique<absl::BlockingCounter>(burst_size_));
  }
  std::vector<privacy_sandbox::server_common::Stopwatch> burst_stopwatches(
      num_bursts_);
  privacy_sandbox::server_common::Stopwatch stopwatch;
  absl::Time expected_start = absl::Now();
  auto latencies_it = stats.invocation_latencies.begin();
  auto outputs_it = stats.invocation_outputs.begin();

  int i;
  for (i = 0; i < num_bursts_ && absl::Now() < end_time; i++) {
    std::string id = absl::StrCat("b", i);
    absl::Duration wait_time = expected_start - absl::Now();
    if (wait_time < absl::ZeroDuration()) {
      if (i > 0) {
        stats.late_count++;
      }
    } else {
      absl::SleepFor(wait_time);
    }
    burst_stopwatches[i].Reset();
    absl::Duration gen_latency =
        Generate(id, &*latencies_it, &*outputs_it, blocking_counters[i].get(),
                 &burst_stopwatches[i], &stats.burst_processing_latencies[i],
                 &stats.wait_latencies[i]);
    stats.burst_creation_latencies.push_back(gen_latency);
    stats.total_invocation_count += burst_size_;
    latencies_it += burst_size_;
    outputs_it += burst_size_;
    expected_start += cadence_;
  }

  stats.total_elapsed = stopwatch.GetElapsedTime();

  for (int j = i; j < num_bursts_; j++) {
    while (!blocking_counters[j]->DecrementCount()) {
    }
  }

  if (i < num_bursts_) {
    stats.total_bursts = i;
    stats.invocation_latencies.resize(i * burst_size_);
    stats.invocation_outputs.resize(i * burst_size_);
    num_bursts_ = i;
  }

  // Wait for all RPCs to complete before stopping the service
  LOG(INFO) << "Waiting for all RPCs to complete...";
  for (auto& counter : blocking_counters) {
    counter->Wait();
  }
  return stats;
}

absl::Duration BurstGenerator::Generate(
    std::string burst_id, absl::StatusOr<absl::Duration>* latencies_ptr,
    absl::StatusOr<std::string>* outputs_ptr, absl::BlockingCounter* counter,
    privacy_sandbox::server_common::Stopwatch* burst_stopwatch,
    absl::Duration* burst_duration,
    absl::StatusOr<absl::Duration>* wait_duration) {
  privacy_sandbox::server_common::Stopwatch stopwatch;
  const std::string qid = absl::StrCat(id_, "-", burst_id);
  for (int i = 0; i < burst_size_; ++i) {
    privacy_sandbox::server_common::Stopwatch fn_stopwatch;
    func_(std::move(fn_stopwatch), latencies_ptr++, outputs_ptr++, counter,
          burst_stopwatch, burst_duration, wait_duration);
  }
  return stopwatch.GetElapsedTime();
}

}  // namespace google::scp::roma::traffic_generator
