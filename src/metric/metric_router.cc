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

#include "src/metric/metric_router.h"

#include <utility>
#include <vector>

#include "absl/log/absl_log.h"
#include "opentelemetry/sdk/metrics/aggregation/histogram_aggregation.h"
#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "src/metric/definition.h"
#include "src/util/status_macro/status_macros.h"

namespace privacy_sandbox::server_common::metrics {
namespace sdk = ::opentelemetry::sdk::metrics;

MetricRouter::MetricRouter(
    std::unique_ptr<MeterProvider> provider, std::string_view service,
    std::string_view version, PrivacyBudget fraction,
    std::unique_ptr<telemetry::BuildDependentConfig> config)
    : provider_(std::move(provider)),
      metric_config_(std::move(config)),
      dp_(this, fraction) {
  if (!provider_) {
    ABSL_LOG(WARNING)
        << "MeterProvider is null at initializing, init with default";
    provider_ = std::make_unique<sdk::MeterProvider>();
  }
  meter_ = provider_->GetMeter(service.data(), version.data()).get();
}

void MetricRouter::AddHistogramView(
    std::string_view instrument_name,
    absl::Span<const double> histogram_boundaries) {
  auto aggregation_config = std::make_shared<sdk::HistogramAggregationConfig>();
  aggregation_config->boundaries_ = std::vector<double>(
      histogram_boundaries.begin(), histogram_boundaries.end());
  auto* sdk_meter = static_cast<sdk::Meter*>(meter_);
  static_cast<sdk::MeterProvider*>(provider_.get())
      ->AddView(
          std::make_unique<sdk::InstrumentSelector>(
              sdk::InstrumentType::kHistogram, instrument_name.data()),
          std::make_unique<sdk::MeterSelector>(
              sdk_meter->GetInstrumentationScope()->GetName(),
              sdk_meter->GetInstrumentationScope()->GetVersion(),
              sdk_meter->GetInstrumentationScope()->GetSchemaURL()),
          // First 2 arguments use empty string, so not to overwrite
          // instrument's name and description
          std::make_unique<sdk::View>("", "", sdk::AggregationType::kHistogram,
                                      aggregation_config));
}
}  // namespace privacy_sandbox::server_common::metrics
