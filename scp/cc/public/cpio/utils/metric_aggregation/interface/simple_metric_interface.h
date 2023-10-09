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
#pragma once

#include <memory>

#include "public/core/interface/execution_result.h"

#include "type_def.h"

namespace google::scp::cpio {
/**
 * @brief Provides a simple metric service. It pushes the single metric
 * data point for the target server to the cloud server.
 */
class SimpleMetricInterface : public core::ServiceInterface {
 public:
  virtual ~SimpleMetricInterface() = default;

  /**
   * @brief Schedules a simple metric push.
   *
   * @param metric_value The value of the metric that will be sent to
   * a monitoring system.
   * @param metric_info This parameter is optional. If you provide a metric_info
   * object, the metric will be created based on the provided metric_info and
   * value. Otherwise, the metric will be created using the predefined
   * metric_info from when the SimpleMetric object was created and pushed to the
   * monitoring system.
   */
  virtual void Push(
      const MetricValue& metric_value,
      std::optional<std::reference_wrapper<const MetricDefinition>>
          metric_info = std::nullopt) noexcept = 0;
};

}  // namespace google::scp::cpio
