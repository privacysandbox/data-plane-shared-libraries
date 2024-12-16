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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
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

}  // namespace

namespace privacy_sandbox::server_common::byob {

std::string BurstGenerator::Stats::ToString() const {
  Percentiles<absl::Duration> burst_ptiles = get_percentiles(burst_latencies);
  Percentiles<absl::Duration> invocation_ptiles =
      get_status_percentiles(invocation_latencies);
  return absl::StrCat(
      "total runtime: ", total_elapsed,
      ", invocation count: ", total_invocation_count,
      " late bursts: ", late_count, "\nburst latencies",
      "\n  count: ", burst_ptiles.count, "\n  min: ", burst_ptiles.min,
      "\n  p50: ", burst_ptiles.p50, "\n  p90: ", burst_ptiles.p90,
      "\n  p95: ", burst_ptiles.p95, "\n  p99: ", burst_ptiles.p99,
      "\n  max: ", burst_ptiles.max, "\ninvocation latencies",
      "\n  count: ", invocation_ptiles.count,
      "\n  min: ", invocation_ptiles.min, "\n  p50: ", invocation_ptiles.p50,
      "\n  p90: ", invocation_ptiles.p90, "\n  p95: ", invocation_ptiles.p95,
      "\n  p99: ", invocation_ptiles.p99, "\n  max: ", invocation_ptiles.max);
}

BurstGenerator::Stats BurstGenerator::Run() const {
  Stats stats(burst_size_, num_bursts_);
  LOG(INFO) << "starting burst generator run."
            << "\n  num bursts: " << num_bursts_
            << "\n  burst cadence: " << cadence_
            << "\n  burst size: " << burst_size_ << std::endl;
  privacy_sandbox::server_common::Stopwatch stopwatch;
  absl::Time expected_start = absl::Now();
  auto latencies_it = stats.invocation_latencies.begin();
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
    absl::Duration gen_latency = Generate(id, &*latencies_it);
    stats.burst_latencies.push_back(gen_latency);
    stats.total_invocation_count += burst_size_;
    latencies_it += burst_size_;
    expected_start += cadence_;
  }
  stats.total_elapsed = stopwatch.GetElapsedTime();
  return stats;
}

absl::Duration BurstGenerator::Generate(
    std::string burst_id, absl::StatusOr<absl::Duration>* latencies_ptr) const {
  privacy_sandbox::server_common::Stopwatch stopwatch;
  const std::string qid = absl::StrCat(id_, "-", burst_id);
  for (int i = 0; i < burst_size_; ++i) {
    privacy_sandbox::server_common::Stopwatch fn_stopwatch;
    func_(std::move(fn_stopwatch), latencies_ptr++);
  }
  return stopwatch.GetElapsedTime();
}

}  // namespace privacy_sandbox::server_common::byob
