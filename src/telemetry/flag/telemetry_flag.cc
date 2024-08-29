// Copyright 2023 Google LLC
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

#include "src/telemetry/flag/telemetry_flag.h"

#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "google/protobuf/text_format.h"

namespace privacy_sandbox::server_common::telemetry {
namespace {

template <typename T>
inline absl::StatusOr<T> ParseText(std::string_view text) {
  T message;
  if (!google::protobuf::TextFormat::ParseFromString(text.data(), &message)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid proto format:{", text, "}"));
  }
  return message;
}

}  // namespace

bool AbslParseFlag(std::string_view text, TelemetryFlag* flag,
                   std::string* err) {
  absl::StatusOr<TelemetryConfig> s = ParseText<TelemetryConfig>(text);
  if (!s.ok()) {
    *err = s.status().message();
    return false;
  }
  flag->server_config = *s;
  return true;
}

std::string AbslUnparseFlag(const TelemetryFlag& flag) {
  return flag.server_config.ShortDebugString();
}

BuildDependentConfig::BuildDependentConfig(TelemetryConfig config)
    : server_config_(std::move(config)) {
  if (server_config_.metric_export_interval_ms() == 0) {
    constexpr int kDefaultMetricExport = 60'000;
    server_config_.set_metric_export_interval_ms(kDefaultMetricExport);
  }
  if (server_config_.dp_export_interval_ms() == 0) {
    constexpr int kDefaultDpExport = 300'000;
    server_config_.set_dp_export_interval_ms(kDefaultDpExport);
  }
  // dp_export_interval_ms should be at least metric_export_interval_ms
  if (server_config_.metric_export_interval_ms() >
      server_config_.dp_export_interval_ms()) {
    server_config_.set_dp_export_interval_ms(
        server_config_.metric_export_interval_ms());
  }
  absl::BitGen bitgen;
  server_config_.set_dp_export_interval_ms(
      server_config_.dp_export_interval_ms() * absl::Uniform(bitgen, 1, 1.1));

  for (const MetricConfig& m : server_config_.metric()) {
    metric_config_.emplace(m.name(), m);
  }
  for (const auto& m : server_config_.custom_metric()) {
    auto metric = SetCustomConfig(m);
    if (!metric.ok()) {
      if (metric.code() == absl::StatusCode::kAlreadyExists) {
        continue;
      } else {
        ABSL_LOG(ERROR) << metric.message();
        break;
      }
    }
  }
}

TelemetryConfig::TelemetryMode BuildDependentConfig::MetricMode() const {
  if (GetBuildMode() == BuildMode::kExperiment) {
    return server_config_.mode();
  } else {
    return server_config_.mode() == TelemetryConfig::OFF
               ? TelemetryConfig::OFF
               : TelemetryConfig::PROD;
  }
}

bool BuildDependentConfig::MetricAllowed() const {
  switch (server_config_.mode()) {
    case TelemetryConfig::PROD:
    case TelemetryConfig::EXPERIMENT:
    case TelemetryConfig::COMPARE:
      return true;
    default:
      return false;
  }
}

bool BuildDependentConfig::IsDebug() const {
  switch (server_config_.mode()) {
    case TelemetryConfig::EXPERIMENT:
    case TelemetryConfig::COMPARE:
      return GetBuildMode() == BuildMode::kExperiment;
    default:
      return false;
  }
}

absl::StatusOr<MetricConfig> BuildDependentConfig::GetMetricConfig(
    std::string_view metric_name) const {
  if (metric_config_.empty()) {
    return MetricConfig();
  }
  auto it = metric_config_.find(metric_name);
  if (it == metric_config_.end()) {
    return absl::NotFoundError(metric_name);
  }
  return it->second;
}

absl::Status BuildDependentConfig::CheckMetricConfig(
    absl::Span<const metrics::DefinitionName* const> server_metrics) const {
  std::string ret;
  for (const auto& [name, config] : metric_config_) {
    if (absl::c_find_if(server_metrics, [&name = name](const auto* metric_def) {
          return metric_def->name_ == name;
        }) == server_metrics.end()) {
      absl::StrAppend(&ret, absl::StrCat(name, " not defined;"));
    }
  }
  return ret.empty() ? absl::OkStatus() : absl::InvalidArgumentError(ret);
}

void BuildDependentConfig::SetPartition(
    std::string_view name, absl::Span<const std::string_view> partitions)
    ABSL_LOCKS_EXCLUDED(partition_mutex_) {
  absl::MutexLock lock(&partition_mutex_);
  auto& saved = *internal_config_[name].mutable_public_partitions();
  saved.Assign(partitions.begin(), partitions.end());
  absl::c_sort(saved);
  partition_config_view_[name] = {saved.begin(), saved.end()};
}

int BuildDependentConfig::GetMaxPartitionsContributed(
    const metrics::internal::Partitioned& definition,
    absl::string_view name) const ABSL_LOCKS_EXCLUDED(partition_mutex_) {
  {
    absl::MutexLock lock(&partition_mutex_);
    auto it = internal_config_.find(name);
    if (it != internal_config_.end() &&
        it->second.has_max_partitions_contributed()) {
      return it->second.max_partitions_contributed();
    }
  }
  absl::StatusOr<MetricConfig> metric_config = GetMetricConfig(name);
  if (metric_config.ok() && metric_config->has_max_partitions_contributed()) {
    return metric_config->max_partitions_contributed();
  }
  return definition.max_partitions_contributed_;
}

void BuildDependentConfig::SetMaxPartitionsContributed(
    std::string_view name, int max_partitions_contributed)
    ABSL_LOCKS_EXCLUDED(partition_mutex_) {
  absl::MutexLock lock(&partition_mutex_);
  internal_config_[name].set_max_partitions_contributed(
      max_partitions_contributed);
}

absl::Status BuildDependentConfig::SetCustomConfig(
    const UDFMetricDefinition& proto) ABSL_LOCKS_EXCLUDED(partition_mutex_) {
  absl::MutexLock lock(&partition_mutex_);
  for (int i = 0; i < CustomMetricCount(); i++) {
    const server_common::metrics::Definition<
        double, server_common::metrics::Privacy::kImpacting,
        server_common::metrics::Instrument::kUpDownCounter>* kCustom =
        server_common::metrics::kCustomList[i];
    if (internal_config_.find(kCustom->name_) == internal_config_.end()) {
      // udf name should be defined only once
      if (GetCustomDefinition(proto.name()) == nullptr) {
        *internal_config_[kCustom->name_].mutable_name() = proto.name();
        *internal_config_[kCustom->name_].mutable_description() =
            proto.description();
        internal_config_[kCustom->name_].set_lower_bound(proto.lower_bound());
        internal_config_[kCustom->name_].set_upper_bound(proto.upper_bound());
        custom_def_map_[proto.name()] = kCustom;
        return absl::OkStatus();
      } else {
        return absl::AlreadyExistsError(
            absl::StrCat(proto.name(), " has already been defined"));
      }
    }
  }
  return absl::ResourceExhaustedError(
      "max number of custom metrics has been reached");
}

absl::StatusOr<const MetricConfig*> BuildDependentConfig::GetInternalConfig(
    std::string_view name) const ABSL_LOCKS_EXCLUDED(partition_mutex_) {
  absl::MutexLock lock(&partition_mutex_);
  auto it = internal_config_.find(name);
  if (it == internal_config_.end()) {
    return absl::NotFoundError(name);
  }
  return &(it->second);
}

std::string_view BuildDependentConfig::GetName(
    const metrics::DefinitionName& definition) const {
  absl::StatusOr<const MetricConfig*> internal_config =
      GetInternalConfig(definition.name_);
  if (internal_config.ok() && (*internal_config)->has_name()) {
    return (*internal_config)->name();
  } else {
    return definition.name_;
  }
}

std::string_view BuildDependentConfig::GetDescription(
    const metrics::DefinitionName& definition) const {
  absl::StatusOr<const MetricConfig*> internal_config =
      GetInternalConfig(definition.name_);
  if (internal_config.ok() && (*internal_config)->has_description()) {
    return (*internal_config)->description();
  } else {
    return definition.description_;
  }
}

}  // namespace privacy_sandbox::server_common::telemetry
