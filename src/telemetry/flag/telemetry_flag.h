/*
 * Copyright 2023 Google LLC
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

#ifndef SERVICES_COMMON_TELEMETRY_TELEMETRY_FLAG_H_
#define SERVICES_COMMON_TELEMETRY_TELEMETRY_FLAG_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "src/metric/definition.h"
#include "src/telemetry/flag/config.pb.h"

namespace privacy_sandbox::server_common::telemetry {

struct TelemetryFlag {
  TelemetryConfig server_config;
};

bool AbslParseFlag(std::string_view text, TelemetryFlag* flag,
                   std::string* err);

std::string AbslUnparseFlag(const TelemetryFlag&);

// BuildDependentConfig wrap `TelemetryConfig`, provide methods that also
// depends on the build options `//:non_prod_build`
class BuildDependentConfig {
 public:
  struct BoundOverride {
    double lower_bound_;
    double upper_bound_;
  };

  // The borrowed view should not be invalidated until `lock` goes out of scope.
  class PartitionView {
   public:
    PartitionView(const BuildDependentConfig& config,
                  const metrics::internal::Partitioned& definition,
                  const std::string_view name)
        : lock_(&config.partition_mutex_) {
      auto it = config.partition_config_view_.find(name);
      if (it == config.partition_config_view_.end()) {
        view_ = definition.public_partitions_;
      } else {
        view_ = it->second;
      }
    }

    absl::Span<const std::string_view> view() const { return view_; }

   private:
    absl::Span<const std::string_view> view_;
    absl::MutexLock lock_;
  };

  explicit BuildDependentConfig(TelemetryConfig config);

  enum class BuildMode { kProd, kExperiment };

  // Get build mode. The implementation depend on build flag.
  BuildMode GetBuildMode() const;

  // Get the metric mode to use
  TelemetryConfig::TelemetryMode MetricMode() const;

  // Should metric be collected and exported;
  // Used to initialize Open Telemetry.
  bool MetricAllowed() const;

  // Should trace be collected and exported;
  // Used to initialize Open Telemetry.
  bool TraceAllowed() const { return IsDebug(); }

  // Should logs be collected and exported;
  // Used to initialize Open Telemetry.
  bool LogsAllowed() const {
    return server_config_.mode() != TelemetryConfig::OFF;
  }

  // Should metric collection run as debug mode(without noise)
  bool IsDebug() const;

  int metric_export_interval_ms() const {
    return server_config_.metric_export_interval_ms();
  }

  int dp_export_interval_ms() const {
    return server_config_.dp_export_interval_ms();
  }

  // If server_config_ has defined MetricConfig list, if found return the
  // MetricConfig for `metric_name`, otherwise return error; if server_config_
  // has empty MetricConfig list, always return default MetricConfig.
  absl::StatusOr<MetricConfig> GetMetricConfig(
      std::string_view metric_name) const;

  // return error if metric is not configured right.
  absl::Status CheckMetricConfig(
      absl::Span<const metrics::DefinitionName* const> server_metrics) const;

  // Override the public partition of a metric. Unless used at server startup
  // time, setting partition should also involve clearing
  // DifferentiallyPrivate's counter. ResetPartitionAsync in
  // DifferentiallyPrivate should be called to reset metric partation
  // dynamically in an atomic fashion.

  void SetPartition(std::string_view name,
                    absl::Span<const std::string_view> partitions)
      ABSL_LOCKS_EXCLUDED(partition_mutex_);

  void SetPartitionWithMutex(std::string_view name,
                             absl::Span<const std::string_view> partitions)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(partition_mutex_);

  // Return the public partition of a metric.
  template <typename MetricT>
  std::unique_ptr<PartitionView> GetPartition(const MetricT& definition) const {
    return GetPartition(definition, definition.name_);
  }

  // Return the public partition of a metric. The partition is valid as long
  // as the PartitionView object is in scope.
  std::unique_ptr<PartitionView> GetPartition(
      const metrics::internal::Partitioned& definition,
      const std::string_view name) const ABSL_LOCKS_EXCLUDED(partition_mutex_) {
    return std::make_unique<PartitionView>(*this, definition, name);
  }

  // Return max_partions_contributed of a metric.
  template <typename MetricT>
  int GetMaxPartitionsContributed(const MetricT& definition) const {
    return GetMaxPartitionsContributed(definition, definition.name_);
  }

  int GetMaxPartitionsContributed(
      const metrics::internal::Partitioned& definition,
      absl::string_view name) const ABSL_LOCKS_EXCLUDED(partition_mutex_);

  void SetMaxPartitionsContributed(std::string_view name,
                                   int max_partitions_contributed)
      ABSL_LOCKS_EXCLUDED(partition_mutex_);

  // Return partion_type of a metric.
  template <typename MetricT>
  std::string GetPartitionType(const MetricT& definition) const {
    return GetPartitionType(definition, definition.name_);
  }

  std::string GetPartitionType(const metrics::internal::Partitioned& definition,
                               absl::string_view name) const;

  // Return drop_noisy_values_probability of a metric.
  template <typename MetricT>
  double GetDropNoisyValuesProbability(const MetricT& definition) const {
    return GetDropNoisyValuesProbability(definition, definition.name_);
  }

  template <typename T>
  double GetDropNoisyValuesProbability(
      const metrics::internal::DifferentialPrivacy<T>& definition,
      absl::string_view name) const {
    absl::StatusOr<MetricConfig> metric_config = GetMetricConfig(name);
    if (metric_config.ok() &&
        metric_config->has_drop_noisy_values_probability()) {
      return metric_config->drop_noisy_values_probability();
    }
    return definition.drop_noisy_values_probability_;
  }

  // Return privacy_budget_weight of a metric.
  template <typename MetricT>
  double GetPrivacyBudgetWeight(const MetricT& definition) const {
    return GetPrivacyBudgetWeight(definition, definition.name_);
  }

  template <typename T>
  double GetPrivacyBudgetWeight(
      const metrics::internal::DifferentialPrivacy<T>& definition,
      absl::string_view name) const {
    absl::MutexLock lock(&partition_mutex_);
    auto it = internal_config_.find(name);
    if (it != internal_config_.end() &&
        it->second.has_privacy_budget_weight()) {
      return it->second.privacy_budget_weight();
    }
    absl::StatusOr<MetricConfig> metric_config = GetMetricConfig(name);
    if (metric_config.ok() && metric_config->has_privacy_budget_weight()) {
      return metric_config->privacy_budget_weight();
    }
    return definition.privacy_budget_weight_;
  }

  absl::Status SetCustomConfig(const MetricConfig& proto)
      ABSL_LOCKS_EXCLUDED(partition_mutex_);

  absl::StatusOr<
      const metrics::Definition<double, metrics::Privacy::kImpacting,
                                metrics::Instrument::kPartitionedCounter>*>
  GetCustomDefinition(absl::string_view udf_name) {
    auto it = custom_def_map_.find(udf_name);
    if (it == custom_def_map_.end()) {
      return absl::NotFoundError(udf_name);
    }
    return it->second;
  }

  std::string GetName(const metrics::DefinitionName& definition) const;

  std::string GetDescription(const metrics::DefinitionName& definition) const;

  // Total privacy budget weight of all defined custom metrics.
  double CustomMetricsWeight() {
    double total = 0.0;
    for (const auto& def : custom_def_map_) {
      total += GetPrivacyBudgetWeight(*def.second);
    }
    return total;
  }

  // Return lower_bound and upper_bound of a metric.
  template <typename MetricT>
  BoundOverride GetBound(const MetricT& definition) const {
    return GetBound(definition, definition.name_);
  }

  template <typename T>
  BoundOverride GetBound(
      const metrics::internal::DifferentialPrivacy<T>& definition,
      absl::string_view name) const {
    double new_lower_bound = definition.lower_bound_;
    double new_upper_bound = definition.upper_bound_;
    absl::MutexLock lock(&partition_mutex_);
    auto it = internal_config_.find(name);
    if (it != internal_config_.end()) {
      if (it->second.has_lower_bound()) {
        new_lower_bound = it->second.lower_bound();
      }
      if (it->second.has_upper_bound()) {
        new_upper_bound = it->second.upper_bound();
      }
    } else {
      // Only check metric config if internal config is not found.
      absl::StatusOr<MetricConfig> metric_config = GetMetricConfig(name);
      if (metric_config.ok()) {
        if (metric_config->has_lower_bound()) {
          new_lower_bound = metric_config->lower_bound();
        }
        if (metric_config->has_upper_bound()) {
          new_upper_bound = metric_config->upper_bound();
        }
      }
    }
    if (new_lower_bound < new_upper_bound) {
      return BoundOverride{
          .lower_bound_ = new_lower_bound,
          .upper_bound_ = new_upper_bound,
      };
    }
    return BoundOverride{
        .lower_bound_ = double(definition.lower_bound_),
        .upper_bound_ = double(definition.upper_bound_),
    };
  }

 private:
  TelemetryConfig server_config_;
  absl::flat_hash_map<std::string, MetricConfig> metric_config_;
  mutable absl::Mutex partition_mutex_;
  absl::flat_hash_map<std::string, MetricConfig> internal_config_
      ABSL_GUARDED_BY(partition_mutex_);
  absl::flat_hash_map<std::string, std::vector<std::string_view>>
      partition_config_view_ ABSL_GUARDED_BY(partition_mutex_);

  absl::flat_hash_map<
      std::string,
      const metrics::Definition<double, metrics::Privacy::kImpacting,
                                metrics::Instrument::kPartitionedCounter>*>
      custom_def_map_;
};

}  // namespace privacy_sandbox::server_common::telemetry

#endif  // SERVICES_COMMON_TELEMETRY_TELEMETRY_FLAG_H_
