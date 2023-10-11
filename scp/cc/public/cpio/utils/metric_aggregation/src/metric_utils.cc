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

#include "metric_utils.h"

#include <utility>

namespace {
constexpr char kMethodName[] = "MethodName";
constexpr char kComponentName[] = "ComponentName";
}  // namespace

namespace google::scp::cpio {

void MetricUtils::GetPutMetricsRequest(
    std::shared_ptr<cmrt::sdk::metric_service::v1::PutMetricsRequest>&
        record_metric_request,
    const MetricDefinition& metric_info,
    const MetricValue& metric_value) noexcept {
  auto metric = record_metric_request->add_metrics();
  metric->set_value(metric_value);
  metric->set_name(metric_info.name);
  metric->set_unit(
      client_providers::MetricClientUtils::ConvertToMetricUnitProto(
          metric_info.unit));

  // Adds the labels from metric_info and additional_labels.
  auto& labels = *metric->mutable_labels();
  if (metric_info.labels.size() > 0) {
    for (const auto& label : metric_info.labels) {
      labels[label.first] = label.second;
    }
  }

  *metric->mutable_timestamp() = protobuf::util::TimeUtil::GetCurrentTime();
  record_metric_request->set_metric_namespace(metric_info.metric_namespace);
}

/**
 * @brief Registers a simple metric with MetricClient.
 *
 * @param async_executor
 * @param metric_client
 * @param metric_name Name of the metric
 * @param metric_label_component Component Name where the metric is emitted
 * @param metric_label_method Method Name where the metric is emitted
 * @param metric_unit_type unit type
 * @return std::shared_ptr<SimpleMetricInterface>
 */
std::shared_ptr<SimpleMetricInterface> MetricUtils::RegisterSimpleMetric(
    const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
    const std::shared_ptr<MetricClientInterface>& metric_client,
    const std::string& metric_name, const std::string& metric_label_component,
    const std::string& metric_label_method,
    MetricUnit metric_unit_type) noexcept {
  auto metric_labels = MetricUtils::CreateMetricLabelsWithComponentSignature(
      metric_label_component, metric_label_method);
  auto metric_info =
      MetricDefinition(metric_name, metric_unit_type, kDefaultMetricNamespace,
                       std::move(metric_labels));
  return std::make_shared<SimpleMetric>(async_executor, metric_client,
                                        std::move(metric_info));
}

/**
 * @brief Registers a aggregate metric with MetricClient.
 *
 * @param async_executor
 * @param metric_client
 * @param metric_name Name of the metric
 * @param metric_label_component Component Name where the metric is emitted
 * @param metric_label_method Method Name where the metric is emitted
 * @param metric_unit_type unit type
 * @param metric_event_labels Dimension labels of the metric
 * @param aggregated_metric_interval_ms Aggregation interval
 * @return std::shared_ptr<AggregateMetricInterface>
 */
std::shared_ptr<AggregateMetricInterface> MetricUtils::RegisterAggregateMetric(
    const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
    const std::shared_ptr<MetricClientInterface>& metric_client,
    const std::string& metric_name, const std::string& metric_label_component,
    const std::string& metric_label_method, MetricUnit metric_unit_type,
    std::vector<std::string> metric_event_labels,
    size_t aggregated_metric_interval_ms) noexcept {
  auto metric_labels = MetricUtils::CreateMetricLabelsWithComponentSignature(
      metric_label_component, metric_label_method);
  auto metric_info =
      MetricDefinition(metric_name, metric_unit_type, kDefaultMetricNamespace,
                       std::move(metric_labels));
  return std::make_shared<AggregateMetric>(
      async_executor, metric_client, std::move(metric_info),
      aggregated_metric_interval_ms, metric_event_labels);
}

MetricLabels MetricUtils::CreateMetricLabelsWithComponentSignature(
    std::string component_name, std::string method_name) noexcept {
  MetricLabels labels;
  labels[kComponentName] = std::move(component_name);
  if (!method_name.empty()) {
    labels[kMethodName] = std::move(method_name);
  }

  return labels;
}
}  // namespace google::scp::cpio
