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

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aggregate_metric_interface.h"
#include "simple_metric_interface.h"
#include "type_def.h"

namespace google::scp::cpio {

/**
 * @brief A MetricInstanceFactory makes it simple to create metric instances for
 * system monitoring.
 */
class MetricInstanceFactoryInterface {
 public:
  virtual ~MetricInstanceFactoryInterface() = default;

  /**
   * @brief Construct a SimpleMetric instance.
   *
   * @param metric_info the basic metric information for the simple metric
   * instance.
   * @return std::unique_ptr<SimpleMetricInterface> an instance of SimpleMetric.
   */
  virtual std::unique_ptr<SimpleMetricInterface> ConstructSimpleMetricInstance(
      MetricDefinition metric_info) noexcept = 0;

  /**
   * @brief Construct an AggregateMetric instance. This AggregateMetric instance
   * will track a single metric, as defined by the metric_info.
   *
   * @param metric_info The definition of the metric that the AggregateMetric
   * monitors.
   * @return std::unique_ptr<AggregateMetricInterface>
   */
  virtual std::unique_ptr<AggregateMetricInterface>
  ConstructAggregateMetricInstance(MetricDefinition metric_info) noexcept = 0;

  /**
   * @brief Construct an AggregateMetric instance. The AggregateMetric will
   * track a specific set of event metrics, which are defined by the metric_info
   * and event_code_labels_list.
   *
   * @param metric_info the basic metric information for the aggregate metric
   * instance.
   * @param event_code_labels_list The event labels associated with the set of
   * metrics, where each metric has a unique event label.
   * @param event_code_name The name of the event code for which the
   * AggregateMetric will track. If no even_code_name provided, the default
   * event_name in AggregateMetric will be used.
   * @return std::unique_ptr<AggregateMetricInterface> an instance of
   * AggregateMetric.
   */
  virtual std::unique_ptr<AggregateMetricInterface>
  ConstructAggregateMetricInstance(
      MetricDefinition metric_info,
      const std::vector<std::string>& event_code_labels_list,
      const std::string& event_code_name = std::string()) noexcept = 0;
};
}  // namespace google::scp::cpio
