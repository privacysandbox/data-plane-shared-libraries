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

#ifndef PUBLIC_CPIO_UTILS_METRIC_AGGREGATION_INTERFACE_TYPE_DEF_H_
#define PUBLIC_CPIO_UTILS_METRIC_AGGREGATION_INTERFACE_TYPE_DEF_H_

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "src/core/interface/type_def.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/metric_client/type_def.h"
#include "src/public/cpio/proto/metric_service/v1/metric_service.pb.h"

namespace google::scp::cpio {
using MetricNamespace = std::string;
using MetricName = std::string;
using MetricValue = std::string;
using MetricLabels = absl::flat_hash_map<std::string, std::string>;
/// Supported metric units.
enum class MetricUnit {
  kUnknown = 0,
  kSeconds = 1,
  kMicroseconds = 2,
  kMilliseconds = 3,
  kBits = 4,
  kKilobits = 5,
  kMegabits = 6,
  kGigabits = 7,
  kTerabits = 8,
  kBytes = 9,
  kKilobytes = 10,
  kMegabytes = 11,
  kGigabytes = 12,
  kTerabytes = 13,
  kCount = 14,
  kPercent = 15,
  kBitsPerSecond = 16,
  kKilobitsPerSecond = 17,
  kMegabitsPerSecond = 18,
  kGigabitsPerSecond = 19,
  kTerabitsPerSecond = 20,
  kBytesPerSecond = 21,
  kKilobytesPerSecond = 22,
  kMegabytesPerSecond = 23,
  kGigabytesPerSecond = 24,
  kTerabytesPerSecond = 25,
  kCountPerSecond = 26,
};

/// A default metric namespace for MetricDefinition creation.
inline constexpr std::string_view kDefaultMetricNamespace =
    "DefaultMetricNamespace";

/// Represents the metric definition.
struct MetricDefinition {
  explicit MetricDefinition(MetricName metric_name, MetricUnit metric_unit,
                            MetricNamespace input_namespace)
      : name(std::move(metric_name)),
        unit(metric_unit),
        metric_namespace(std::move(input_namespace)) {}

  explicit MetricDefinition(MetricName metric_name, MetricUnit metric_unit,
                            MetricNamespace input_namespace,
                            MetricLabels metric_labels)
      : name(std::move(metric_name)),
        unit(metric_unit),
        labels(std::move(metric_labels)),
        metric_namespace(std::move(input_namespace)) {}

  void AddMetricLabels(MetricLabels metric_labels) {
    labels.insert(std::make_move_iterator(metric_labels.begin()),
                  std::make_move_iterator(metric_labels.end()));
  }

  /// Metric name.
  MetricName name;
  /// Metric unit.
  MetricUnit unit;
  /// A set of key-value pairs. The key represents label name and the value
  /// represents label value.
  MetricLabels labels;
  /// The namespace parameter required for pushing metric data to AWS.
  MetricNamespace metric_namespace;
};

/**
 * @brief The structure of TimeEvent which used to record the start time, end
 * time and difference time for one event.
 *
 */
struct TimeEvent {
  /**
   * @brief Construct a new Time Event object. The start_time is the time when
   * object constructed.
   */
  TimeEvent() {
    start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now().time_since_epoch())
                     .count();
  }

  /**
   * @brief When Stop() is called, the end_time is recorded and also the
   * diff_time is the difference between end_time and start_time.
   */
  void Stop() {
    end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count();
    diff_time = end_time - start_time;
  }

  /// The start time for the event.
  core::Timestamp start_time;
  /// The end time for the event.
  core::Timestamp end_time;
  /// The duration time for the event.
  core::TimeDuration diff_time;
};

}  // namespace google::scp::cpio

#endif  // PUBLIC_CPIO_UTILS_METRIC_AGGREGATION_INTERFACE_TYPE_DEF_H_
