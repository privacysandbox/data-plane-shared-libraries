//  Copyright 2022 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#ifndef METRIC_DP_H_
#define METRIC_DP_H_

#include <algorithm>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_log.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "algorithms/bounded-sum.h"
#include "src/metric/definition.h"
#include "src/telemetry/flag/telemetry_flag.h"
#include "src/util/status_macro/status_macros.h"

namespace privacy_sandbox::server_common::metrics {

struct PrivacyBudget {
  double epsilon;
};

namespace internal {
// Abstract class of DpAggregator, can output noised result.
class DpAggregatorBase {
 public:
  // Output aggregated results with DP noise added.
  virtual absl::StatusOr<std::vector<differential_privacy::Output>>
  OutputNoised() = 0;
  virtual void Reset() = 0;
  virtual ~DpAggregatorBase() = default;
};

ABSL_CONST_INIT inline bool output_noise_interval = false;

inline absl::flat_hash_map<std::string, std::string> Attributes(
    const differential_privacy::Output& output) {
  if (output_noise_interval) {
    return {
        {kNoiseAttribute.data(), "Noised"},
        {"0.95NoiseConfidenceInterval",
         absl::StrFormat(
             "%.3f", differential_privacy::GetNoiseConfidenceInterval(output)
                         .upper_bound())},
    };
  } else {
    return {{kNoiseAttribute.data(), "Noised"}};
  }
}

// DpAggregator is thread-safe counter to aggregate metric and add noise;
// It should only be constructed from `DifferentiallyPrivate`.
// see `DifferentiallyPrivate` about `TMetricRouter`;
// see `Definition` about `TValue`, `privacy`, `instrument`;
template <typename TMetricRouter, typename TValue, Privacy privacy,
          Instrument instrument>
class DpAggregator : public DpAggregatorBase {
 public:
  DpAggregator(TMetricRouter* metric_router,
               const Definition<TValue, privacy, instrument>* definition,
               PrivacyBudget privacy_budget_per_weight)
      : metric_router_(*metric_router),
        definition_(*definition),
        privacy_budget_per_weight_(privacy_budget_per_weight) {}

  // Aggregate `value` for  a metric. This is only called from
  // `DifferentiallyPrivate`, can be called multiple times before
  // `OutputNoised()` result.  Each `partition` aggregate separately. If not
  // partitioned, `partition` is empty string.
  absl::Status Aggregate(TValue value, std::string_view partition)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock mutex_lock(&mutex_);
    auto it = bounded_sums_.find(partition);
    if (it == bounded_sums_.end()) {
      int max_partitions_contributed;
      const std::string_view current_partition[] = {partition};
      absl::Span<const std::string_view> all_partitions = current_partition;
      // With a partitioned counter, `partition_view` needs to have a larger
      // scope than the following if clause to keep the `all_partitions` valid.
      std::unique_ptr<telemetry::BuildDependentConfig::PartitionView>
          partition_view;
      auto bound =
          metric_router_.metric_config().template GetBound(definition_);
      auto privacy_budget_weight =
          metric_router_.metric_config().template GetPrivacyBudgetWeight(
              definition_);
      if constexpr (instrument == Instrument::kPartitionedCounter) {
        max_partitions_contributed =
            metric_router_.metric_config().template GetMaxPartitionsContributed(
                definition_);
        if (partition_view =
                metric_router_.metric_config().template GetPartition(
                    definition_);
            !partition_view->view().empty()) {
          all_partitions = partition_view->view();
        }
      } else {
        max_partitions_contributed = 1;
      }
      for (absl::string_view each : all_partitions) {
        PS_ASSIGN_OR_RETURN(
            std::unique_ptr<differential_privacy::BoundedSum<TValue>>
                bounded_sum,
            typename differential_privacy::BoundedSum<TValue>::Builder()
                .SetEpsilon(privacy_budget_per_weight_.epsilon *
                            privacy_budget_weight)
                .SetLower(bound.lower_bound_)
                .SetUpper(bound.upper_bound_)
                .SetMaxPartitionsContributed(max_partitions_contributed)
                .SetLaplaceMechanism(
                    std::make_unique<
                        differential_privacy::LaplaceMechanism::Builder>())
                .Build());
        it = bounded_sums_.emplace(each, std::move(bounded_sum)).first;
      }
      if constexpr (instrument == Instrument::kPartitionedCounter) {
        it = bounded_sums_.find(partition);
      }
    }
    it->second->AddEntry(value);
    return absl::OkStatus();
  }

  // After each `OutputNoised`, all aggregated value will be reset.
  absl::StatusOr<std::vector<differential_privacy::Output>> OutputNoised()
      override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock mutex_lock(&mutex_);
    std::vector<differential_privacy::Output> ret(bounded_sums_.size());
    auto it = ret.begin();
    const double drop_noisy_values_probability =
        metric_router_.metric_config().template GetDropNoisyValuesProbability(
            definition_);
    for (auto& [partition, bounded_sum] : bounded_sums_) {
      PS_ASSIGN_OR_RETURN(*it, bounded_sum->PartialResult());
      if ((drop_noisy_values_probability > 0.001) &&
          (drop_noisy_values_probability < 1.0)) {
        PS_ASSIGN_OR_RETURN(
            differential_privacy::ConfidenceInterval noise_bound,
            bounded_sum->NoiseConfidenceInterval(
                drop_noisy_values_probability));
        if (differential_privacy::GetValue<TValue>(*it) <=
            noise_bound.upper_bound()) {
          differential_privacy::Output output =
              differential_privacy::MakeOutput<TValue>(
                  0, differential_privacy::GetNoiseConfidenceInterval(*it));
          *it = output;
        }
      }
      PS_RETURN_IF_ERROR((metric_router_.LogSafe(
          definition_, differential_privacy::GetValue<TValue>(*it), partition,
          Attributes(*it))));
      ++it;
      bounded_sum->Reset();
    }
    return ret;
  }

  void Reset() override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock mutex_lock(&mutex_);
    bounded_sums_.clear();
  }

 private:
  TMetricRouter& metric_router_;
  const Definition<TValue, privacy, instrument>& definition_;
  PrivacyBudget privacy_budget_per_weight_;
  absl::Mutex mutex_;
  absl::flat_hash_map<std::string,
                      std::unique_ptr<differential_privacy::BoundedSum<TValue>>>
      bounded_sums_ ABSL_GUARDED_BY(mutex_);
};

// Get mean value of the boundaries of the bucket, used to log into OTel
// histogram; `index` is the bucket index corresponding to `boundaries`.
// For example, with boundaries [10, 20, 30], the effective buckets are [
// [0,10), [10,20), [20,30), [30, ) ] with index [0, 1, 2, 3],
// BucketMean returns [5, 15, 25, 31] for them. i.e. returns mean for buckets
// within 2 boundaries, +1 for buckets on the end.
inline double BucketMean(int index, absl::Span<const double> boundaries) {
  if (index == 0) {
    return boundaries[0] / 2;
  } else if (index == boundaries.size()) {
    return boundaries.back() + 1;
  } else {
    return (boundaries[index - 1] + boundaries[index]) / 2;
  }
}

// partial specialization of DpAggregator<> for kHistogram
template <typename TMetricRouter, typename TValue, Privacy privacy>
class DpAggregator<TMetricRouter, TValue, privacy, Instrument::kHistogram>
    : public DpAggregatorBase {
 public:
  DpAggregator(
      TMetricRouter* metric_router,
      const Definition<TValue, privacy, Instrument::kHistogram>* definition,
      PrivacyBudget privacy_budget_per_weight)
      : metric_router_(*metric_router),
        definition_(*definition),
        privacy_budget_per_weight_(privacy_budget_per_weight) {
    for (size_t i = 0; i < definition_.histogram_boundaries_.size() + 1; ++i) {
      auto bounded_sum =
          typename differential_privacy::BoundedSum<int>::Builder()
              .SetEpsilon(privacy_budget_per_weight_.epsilon *
                          metric_router_.metric_config()
                              .template GetPrivacyBudgetWeight(definition_))
              .SetLower(0)
              .SetUpper(1)  // histogram count add at most 1 each time
              .SetMaxPartitionsContributed(1)
              .SetLaplaceMechanism(
                  std::make_unique<
                      differential_privacy::LaplaceMechanism::Builder>())
              .Build();
      CHECK_OK(bounded_sum);
      bounded_sums_.push_back(*std::move(bounded_sum));
    }
  }

  // Aggregate `value` for  a histogram. This is only called from
  // `DifferentiallyPrivate`, can be called multiple times before
  // `OutputNoised()` result.  `partition` is not used, since partitioned
  // histogram is not supported
  absl::Status Aggregate(TValue value, std::string_view partition)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock mutex_lock(&mutex_);
    absl::Span<const double> boundaries = definition_.histogram_boundaries_;
    auto bound = metric_router_.metric_config().template GetBound(definition_);
    TValue bounded_value = std::min(std::max(double(value), bound.lower_bound_),
                                    bound.upper_bound_);
    const int index =
        std::lower_bound(boundaries.begin(), boundaries.end(), bounded_value) -
        boundaries.begin();
    bounded_sums_[index]->AddEntry(1);
    return absl::OkStatus();
  }

  // Get noised histogram counts in buckets, output them to OTel with
  //  `BucketMean()` as bucket value. The bucket value has no impact on the
  //  result, as long as it falls into the bucket range. After each
  //  `OutputNoised`, all aggregated value will be reset.
  absl::StatusOr<std::vector<differential_privacy::Output>> OutputNoised()
      override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock mutex_lock(&mutex_);
    std::vector<differential_privacy::Output> ret(bounded_sums_.size());
    auto it = ret.begin();
    absl::Span<const double> boundaries = definition_.histogram_boundaries_;
    auto bound = metric_router_.metric_config().template GetBound(definition_);
    int j = std::lower_bound(boundaries.begin(), boundaries.end(),
                             bound.lower_bound_) -
            boundaries.begin();
    const int upper = std::lower_bound(boundaries.begin(), boundaries.end(),
                                       bound.upper_bound_) -
                      boundaries.begin();
    for (; j <= upper; ++j) {
      auto& bounded_sum = bounded_sums_[j];
      PS_ASSIGN_OR_RETURN(*it, bounded_sum->PartialResult());
      auto bucket_mean =
          (TValue)BucketMean(j, definition_.histogram_boundaries_);
      for (int i = 0, count = differential_privacy::GetValue<int>(*it);
           i < count; ++i) {
        PS_RETURN_IF_ERROR((metric_router_.LogSafe(definition_, bucket_mean, "",
                                                   Attributes(*it))));
      }
      ++it;
      bounded_sum->Reset();
    }
    return ret;
  }

  void Reset() override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock mutex_lock(&mutex_);
    bounded_sums_.clear();
  }

 private:
  TMetricRouter& metric_router_;
  const Definition<TValue, privacy, Instrument::kHistogram>& definition_;
  PrivacyBudget privacy_budget_per_weight_;
  absl::Mutex mutex_;
  std::vector<std::unique_ptr<differential_privacy::BoundedSum<int>>>
      bounded_sums_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace internal

/*
`DifferentiallyPrivate` is the thread safe class to aggregate
`Privacy::kImpacting` metrics and output the noised result periodically.

`TMetricRouter` is thread-safe metric_router implementing following method:
  template <typename TValue, Privacy privacy, Instrument instrument>
  absl::Status TMetricRouter::LogSafe(
      const Definition<TValue, privacy, instrument>& definition, TValue value,
      std::string_view partition,
      absl::flat_hash_map<std::string, std::string> attribute);
*/
template <typename TMetricRouter>
class DifferentiallyPrivate {
 public:
  /*
   privacy_budget_per_weight = total_budget / total_weight
   used for: privacy_budget = privacy_budget_weight * privacy_budget_per_weight;
   `output_period` is the interval to output aggregated and noise result.
   */
  DifferentiallyPrivate(TMetricRouter* metric_router,
                        PrivacyBudget privacy_budget_per_weight)
      : metric_router_(*metric_router),
        privacy_budget_per_weight_(privacy_budget_per_weight),
        output_period_(absl::Milliseconds(
            metric_router_.metric_config().dp_export_interval_ms())),
        run_output_(std::thread([this]() { RunOutput(); })) {}

  // Aggregate value for the `definition`.
  // `definition` not owned, must out live `DifferentiallyPrivate`.
  template <typename TValue, Privacy privacy, Instrument instrument>
  absl::Status Aggregate(
      const Definition<TValue, privacy, instrument>* definition, TValue value,
      std::string_view partition) ABSL_LOCKS_EXCLUDED(mutex_) {
    std::string_view metric_name = definition->name_;
    using CounterT =
        internal::DpAggregator<TMetricRouter, TValue, privacy, instrument>;
    CounterT* counter;
    {
      absl::MutexLock mutex_lock(&mutex_);
      has_data_ = true;
      auto it = counter_.find(metric_name);
      if (it == counter_.end()) {
        it = counter_
                 .emplace(metric_name, std::make_unique<CounterT>(
                                           &metric_router_, definition,
                                           privacy_budget_per_weight_))
                 .first;
      }
      counter = static_cast<CounterT*>(it->second.get());
    }
    return counter->Aggregate(value, partition);
  }

  PrivacyBudget privacy_budget_per_weight() const {
    return privacy_budget_per_weight_;
  }

  // Queues a set partition request until the next metric output.
  void ResetPartitionAsync(const std::vector<std::string_view>& metric_list,
                           const std::vector<std::string>& partition_list,
                           int max_partions_contributed)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock mutex_lock(&mutex_);
    reset_partition_request_queue_.push(
        {metric_list, partition_list, max_partions_contributed});
  }

  ~DifferentiallyPrivate() {
    stop_signal_.Notify();
    run_output_.join();
  }

 private:
  friend class NoNoiseTest_DifferentiallyPrivate_Test;
  friend class NoNoiseTest_DifferentiallyPrivateResetPartition_Test;

  struct ResetPartitionRequest {
    // Metrics have program lifetime, so we can use std::string_view for metric
    // name.
    std::vector<std::string_view> metric_list;
    std::vector<std::string> partition_list;
    int max_partitions_contributed;
  };

  // Output aggregated results with DP noise added for all definitions with
  // logged metric.
  absl::StatusOr<absl::flat_hash_map<std::string_view,
                                     std::vector<differential_privacy::Output>>>
  OutputNoised() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    // ToDo(b/279955396): lock telemetry export when OutputNoised runs
    absl::flat_hash_map<std::string_view,
                        std::vector<differential_privacy::Output>>
        ret;
    if (!has_data_) {
      return ret;
    }
    for (auto& [name, counter] : counter_) {
      PS_ASSIGN_OR_RETURN(std::vector<differential_privacy::Output> output,
                          counter->OutputNoised());
      ret.emplace(name, std::move(output));
    }
    has_data_ = false;
    return ret;
  }

  // Set partition for queued requests
  void HandleResetPartitionRequests() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    while (!reset_partition_request_queue_.empty()) {
      const ResetPartitionRequest& request =
          reset_partition_request_queue_.front();
      for (std::string_view metric_name : request.metric_list) {
        if (auto it = counter_.find(metric_name); it != counter_.end()) {
          it->second->Reset();
        }
        const std::vector<std::string>& partition_list = request.partition_list;
        metric_router_.metric_config().SetPartition(
            metric_name, std::vector<std::string_view>{partition_list.begin(),
                                                       partition_list.end()});
        if (request.max_partitions_contributed > 0) {
          metric_router_.metric_config().SetMaxPartitionsContributed(
              metric_name, request.max_partitions_contributed);
        }
      }
      reset_partition_request_queue_.pop();
    }
  }

  // Periodically output noised result
  void RunOutput() {
    while (true) {
      stop_signal_.WaitForNotificationWithTimeout(output_period_);
      absl::MutexLock lock(&mutex_);
      auto result = OutputNoised();
      ABSL_LOG_IF(ERROR, !result.ok()) << result.status();
      HandleResetPartitionRequests();
      if (stop_signal_.HasBeenNotified()) {
        break;
      }
    }
  }

  TMetricRouter& metric_router_;
  PrivacyBudget privacy_budget_per_weight_;
  absl::Duration output_period_;

  absl::Mutex mutex_;
  absl::flat_hash_map<std::string_view,
                      std::unique_ptr<internal::DpAggregatorBase>>
      counter_ ABSL_GUARDED_BY(mutex_);
  std::queue<ResetPartitionRequest> reset_partition_request_queue_
      ABSL_GUARDED_BY(mutex_);

  absl::Notification stop_signal_;
  std::thread run_output_;
  // Only output to OTel if data has been aggregated (`has_data_` = true)
  // become true when data has been aggregated, become false after output.
  bool has_data_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace privacy_sandbox::server_common::metrics

#endif  // METRIC_DP_H_
