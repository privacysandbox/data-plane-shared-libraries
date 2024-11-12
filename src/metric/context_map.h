/*
 * Copyright 2023 Google LLC
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

#ifndef METRIC_CONTEXT_MAP_H_
#define METRIC_CONTEXT_MAP_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/synchronization/mutex.h"
#include "src/metric/context.h"
#include "src/metric/metric_router.h"
#include "src/telemetry/flag/telemetry_flag.h"

namespace privacy_sandbox::server_common::metrics {

// ContextMap provide a thread-safe map between T* and `Context`.
// T should be a request received by server, i.e. `GenerateBidsRequest`.
// See detail docs about `definition_list` and `U` at `Context`.
template <typename T,
          const absl::Span<const DefinitionName* const>& definition_list,
          typename U>
class ContextMap {
 public:
  using ContextT = Context<definition_list, U>;
  using SafeContextT = Context<definition_list, U, /*safe_metric_only=*/true>;

  explicit ContextMap(std::unique_ptr<U> metric_router)
      : metric_router_(std::move(metric_router)),
        safe_metric_(SafeContextT::GetContext(metric_router_.get())) {
    CHECK_OK(CheckListOrder());
    CHECK_OK(CheckDropNoisyValuesProbability());
  }

  ~ContextMap() = default;

  // ContextMap is neither copyable nor movable.
  ContextMap(const ContextMap&) = delete;
  ContextMap& operator=(const ContextMap&) = delete;

  // Get the `Context` tied to a T*, create new if not exist, `ContextMap` owns
  // the `Context`.
  ContextT& Get(T* t) ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock mutex_lock(&mutex_);
    auto it = context_.find(t);
    if (it == context_.end()) {
      it =
          context_.emplace(t, ContextT::GetContext(metric_router_.get())).first;
    }
    return *it->second;
  }

  // Release the ownership of the `Context` tied to a T* and return it.
  absl::StatusOr<std::unique_ptr<ContextT>> Remove(T* t)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock mutex_lock(&mutex_);
    auto it = context_.find(t);
    if (it == context_.end()) {
      return absl::NotFoundError("Metric context not found");
    }
    std::unique_ptr<ContextT> c = std::move(it->second);
    context_.erase(it);
    return c;
  }

  template <typename TDefinition>
  absl::Status AddObserverable(
      const TDefinition& definition,
      absl::flat_hash_map<std::string, double> (*callback)()) {
    return metric_router_->AddObserverable(definition, callback);
  }

  telemetry::BuildDependentConfig& metric_config() {
    return metric_router_->metric_config();
  }

  // Resets Partition for a list of metrics. The metric list should consist of
  // std::string_view of metric names taken from the metric definition.
  void ResetPartitionAsync(const std::vector<std::string_view>& metric_list,
                           const std::vector<std::string>& partition_list,
                           int max_partitions_contributed = 0) {
    metric_router_->ResetPartitionAsync(metric_list, partition_list,
                                        max_partitions_contributed);
  }

  const U& metric_router() const { return *metric_router_.get(); }

  absl::Status CheckListOrder() {
    for (auto* definition : definition_list) {
      if (!std::is_sorted(definition->histogram_boundaries_copy_.begin(),
                          definition->histogram_boundaries_copy_.end())) {
        return absl::InvalidArgumentError(absl::StrCat(
            definition->name_, " histogram boundaries is out of order"));
      }
      if (!std::is_sorted(definition->public_partitions_copy_.begin(),
                          definition->public_partitions_copy_.end())) {
        return absl::InvalidArgumentError(absl::StrCat(
            definition->name_, " public partitions is out of order"));
      }
    }
    return absl::OkStatus();
  }

  absl::Status CheckDropNoisyValuesProbability() {
    for (auto* definition : definition_list) {
      absl::StatusOr<telemetry::MetricConfig> config =
          metric_config().GetMetricConfig(definition->name_);
      if (config.ok() && config->has_drop_noisy_values_probability()) {
        if ((config->drop_noisy_values_probability() < 0.0) ||
            (config->drop_noisy_values_probability() >= 1.0)) {
          return absl::InvalidArgumentError(
              absl::StrCat(definition->name_,
                           " drop_noisy_values_probability is out of range"));
        }
      }
    }
    return absl::OkStatus();
  }

  SafeContextT& SafeMetric() { return *safe_metric_; }

 private:
  std::unique_ptr<U> metric_router_;
  absl::Mutex mutex_;
  absl::flat_hash_map<T*, std::unique_ptr<ContextT>> context_
      ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<SafeContextT> safe_metric_;
};

// Get singleton `ContextMap` for T. First call will initialize.
// `config` must have a value at first call, in following calls can be omitted
// or has a same value. `provider` being null will be initialized with default
// MeterProvider without metric exporting.
template <typename T,
          const absl::Span<const DefinitionName* const>& definition_list>
inline auto* GetContextMap(
    std::unique_ptr<telemetry::BuildDependentConfig> config,
    std::unique_ptr<MetricRouter::MeterProvider> provider,
    std::string_view service, std::string_view version, PrivacyBudget budget) {
  static auto* context_map = [config = std::move(config), &provider, service,
                              version, budget]() mutable {
    CHECK(config) << "cannot be null at initialization";
    absl::Status config_status = config->CheckMetricConfig(definition_list);
    ABSL_LOG_IF(WARNING, !config_status.ok()) << config_status;
    const double total_weight =
        config->CustomMetricsWeight() - CountOfCustomList() +
        absl::c_accumulate(
            definition_list, 0.0,
            [&config](double total, const DefinitionName* definition) {
              double privacy_budget_weight = 0;
              if (absl::StatusOr<telemetry::MetricConfig> metric_config =
                      config->GetMetricConfig(definition->name_);
                  metric_config.ok()) {
                if (metric_config->has_privacy_budget_weight()) {
                  privacy_budget_weight =
                      metric_config->privacy_budget_weight();
                } else {
                  privacy_budget_weight =
                      definition->privacy_budget_weight_copy_;
                }
              }
              return total += privacy_budget_weight;
            });
    budget.epsilon /= total_weight;
    return new ContextMap<T, definition_list, MetricRouter>(
        std::make_unique<MetricRouter>(std::move(provider), service, version,
                                       budget, std::move(config)));
  }();
  CHECK(!config || config->IsDebug() == context_map->metric_config().IsDebug())
      << "Must be null or same value after initialized";
  return context_map;
}

template <const absl::Span<const DefinitionName* const>& definition_list>
using ServerContext = Context<definition_list, MetricRouter>;

template <const absl::Span<const DefinitionName* const>& definition_list>
using ServerSafeContext = Context<definition_list, MetricRouter, true>;

}  // namespace privacy_sandbox::server_common::metrics

#endif  // METRIC_CONTEXT_MAP_H_
