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

#include "metric_instance_factory.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/interface/configuration_keys.h"

#include "aggregate_metric.h"
#include "simple_metric.h"

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::kAggregatedMetricIntervalMs;
using google::scp::core::kDefaultAggregatedMetricIntervalMs;
using google::scp::core::common::kZeroUuid;
using std::make_unique;
using std::move;
using std::optional;
using std::reference_wrapper;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

static constexpr char kMetricInstanceFactory[] = "MetricInstanceFactory";

namespace google::scp::cpio {
MetricInstanceFactory::MetricInstanceFactory(
    const shared_ptr<AsyncExecutorInterface>& async_executor,
    const shared_ptr<MetricClientInterface>& metric_client,
    const shared_ptr<ConfigProviderInterface>& config_provider)
    : async_executor_(async_executor), metric_client_(metric_client) {
  auto execution_result = config_provider->Get(kAggregatedMetricIntervalMs,
                                               aggregated_metric_interval_ms_);
  if (!execution_result.Successful()) {
    // If config is not present, continue with default aggregated interval.
    SCP_INFO(kMetricInstanceFactory, kZeroUuid,
             "%s flag not specified. Set AggregateMetric aggregated time "
             "interval to default value %d ms",
             kAggregatedMetricIntervalMs, kDefaultAggregatedMetricIntervalMs);
    aggregated_metric_interval_ms_ = kDefaultAggregatedMetricIntervalMs;
  }
}

unique_ptr<SimpleMetricInterface>
MetricInstanceFactory::ConstructSimpleMetricInstance(
    MetricDefinition metric_info) noexcept {
  return make_unique<SimpleMetric>(async_executor_, metric_client_,
                                   move(metric_info));
}

unique_ptr<AggregateMetricInterface>
MetricInstanceFactory::ConstructAggregateMetricInstance(
    MetricDefinition metric_info) noexcept {
  return make_unique<AggregateMetric>(async_executor_, metric_client_,
                                      move(metric_info),
                                      aggregated_metric_interval_ms_);
}

unique_ptr<AggregateMetricInterface>
MetricInstanceFactory::ConstructAggregateMetricInstance(
    MetricDefinition metric_info, const vector<string>& event_code_labels_list,
    const std::string& event_code_name) noexcept {
  if (event_code_name.empty()) {
    return make_unique<AggregateMetric>(
        async_executor_, metric_client_, move(metric_info),
        aggregated_metric_interval_ms_, event_code_labels_list);
  } else {
    return make_unique<AggregateMetric>(
        async_executor_, metric_client_, move(metric_info),
        aggregated_metric_interval_ms_, event_code_labels_list,
        event_code_name);
  }
}

}  // namespace google::scp::cpio
