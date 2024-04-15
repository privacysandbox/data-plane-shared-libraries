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

#ifndef SCP_CPIO_INTERFACE_METRIC_CLIENT_INTERFACE_H_
#define SCP_CPIO_INTERFACE_METRIC_CLIENT_INTERFACE_H_

#include <memory>

#include "absl/status/status.h"
#include "src/core/interface/async_context.h"
#include "src/public/cpio/proto/metric_service/v1/metric_service.pb.h"

#include "type_def.h"

namespace google::scp::cpio {
/**
 * @brief Interface responsible for recording custom metrics.
 *
 * Use Create to create the MetricClient. Call Init and Run before actually use
 * it, and call Stop when finish using it.
 */
class MetricClientInterface {
 public:
  virtual ~MetricClientInterface() = default;

  virtual absl::Status Init() noexcept = 0;
  [[deprecated]] virtual absl::Status Run() noexcept = 0;
  [[deprecated]] virtual absl::Status Stop() noexcept = 0;

  /**
   * @brief Records custom metrics on Cloud.
   *
   * @param context put metric operation context.
   * @return absl::Status scheduling result returned synchronously.
   */
  virtual absl::Status PutMetrics(
      core::AsyncContext<
          google::cmrt::sdk::metric_service::v1::PutMetricsRequest,
          google::cmrt::sdk::metric_service::v1::PutMetricsResponse>
          context) noexcept = 0;
};

/// Factory to create MetricClient.
class MetricClientFactory {
 public:
  /**
   * @brief Creates MetricClient.
   *
   * @param options configurations for MetricClient.
   * @return std::unique_ptr<MetricClientInterface> MetricClient object.
   */
  static std::unique_ptr<MetricClientInterface> Create(
      MetricClientOptions options);
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_METRIC_CLIENT_INTERFACE_H_
