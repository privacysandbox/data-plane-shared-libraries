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

#ifndef SRC_ROMA_BYOB_TEST_CONCURRENCY__BURST_GENERATOR_H
#define SRC_ROMA_BYOB_TEST_CONCURRENCY__BURST_GENERATOR_H

#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "src/util/duration.h"

namespace privacy_sandbox::server_common::byob {

class BurstGenerator final {
 public:
  struct Stats {
    explicit Stats(int burst_size, int num_bursts)
        : total_elapsed(absl::ZeroDuration()),
          total_invocation_count(0),
          late_count(0) {
      burst_latencies.reserve(burst_size);
      invocation_latencies.resize(burst_size * num_bursts);
    }

    absl::Duration total_elapsed;
    int64_t total_invocation_count;
    int late_count;
    std::vector<absl::Duration> burst_latencies;
    std::vector<absl::StatusOr<absl::Duration>> invocation_latencies;

    std::string ToString() const;
  };

  BurstGenerator(
      std::string id, int64_t num_bursts, int64_t burst_size,
      absl::Duration cadence,
      absl::AnyInvocable<void(privacy_sandbox::server_common::Stopwatch,
                              absl::StatusOr<absl::Duration>*) const>
          func)
      : id_(std::move(id)),
        num_bursts_(num_bursts),
        burst_size_(burst_size),
        cadence_(std::move(cadence)),
        func_(std::move(func)) {}
  ~BurstGenerator() = default;

  Stats Run() const;
  absl::Duration Generate(std::string burst_id,
                          absl::StatusOr<absl::Duration>* latencies_ptr) const;

 private:
  std::string id_;
  int64_t num_bursts_;
  int64_t burst_size_;
  absl::Duration cadence_;
  absl::AnyInvocable<void(privacy_sandbox::server_common::Stopwatch,
                          absl::StatusOr<absl::Duration>*) const>
      func_;
};

}  // namespace privacy_sandbox::server_common::byob

#endif  // SRC_ROMA_BYOB_TEST_CONCURRENCY__BURST_GENERATOR_H
