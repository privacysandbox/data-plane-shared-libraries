/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PUBLIC_CPIO_UTILS_METRIC_AGGREGATION_MOCK_MOCK_AGGREGATE_METRIC_H_
#define PUBLIC_CPIO_UTILS_METRIC_AGGREGATION_MOCK_MOCK_AGGREGATE_METRIC_H_

#include <memory>
#include <mutex>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"

namespace google::scp::cpio {
class MockAggregateMetric : public AggregateMetricInterface {
 public:
  MockAggregateMetric() {}

  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Increment(
      const std::string& event_code) noexcept override {
    return IncrementBy(1, event_code);
  }

  core::ExecutionResult IncrementBy(
      uint64_t value, const std::string& event_code) noexcept override
      ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    metric_count_map_[event_code] += value;
    return core::SuccessExecutionResult();
  }

  size_t GetCounter(const std::string& event_code = std::string())
      ABSL_LOCKS_EXCLUDED(mutex_) {
    if (event_code.empty()) {
      return 0;
    } else {
      absl::MutexLock lock(&mutex_);
      return metric_count_map_[event_code];
    }
  }

 private:
  absl::Mutex mutex_;
  absl::flat_hash_map<std::string, size_t> metric_count_map_
      ABSL_GUARDED_BY(mutex_);
};
}  // namespace google::scp::cpio

#endif  // PUBLIC_CPIO_UTILS_METRIC_AGGREGATION_MOCK_MOCK_AGGREGATE_METRIC_H_
