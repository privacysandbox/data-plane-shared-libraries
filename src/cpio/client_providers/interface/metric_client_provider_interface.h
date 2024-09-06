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

#ifndef CPIO_CLIENT_PROVIDERS_INTERFACE_METRIC_CLIENT_PROVIDER_INTERFACE_H_
#define CPIO_CLIENT_PROVIDERS_INTERFACE_METRIC_CLIENT_PROVIDER_INTERFACE_H_

#include <memory>
#include <string>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/interface/service_interface.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/metric_client/metric_client_interface.h"
#include "src/public/cpio/interface/metric_client/type_def.h"
#include "src/public/cpio/proto/metric_service/v1/metric_service.pb.h"
#include "src/public/cpio/utils/metric_aggregation/interface/type_def.h"

namespace google::scp::cpio::client_providers {

/// Configurations for batching metrics.
struct MetricBatchingOptions {
  /**
   * @brief The top level grouping for the application metrics. A
   * typical example would be "/application_name/environment_name".
   * Environment name could be fetched through ConfigClient.
   * Batching only works for the metrics for the same namespace.
   */
  MetricNamespace metric_namespace;
  /**
   * @brief Pushes metrics in batches if true. In most times, when the
   * batch_recording_time_duration is met, the push is triggered. Cloud has
   * its own maximum batch size, and if the maximum batch size is met before the
   * batch_recording_time_duration, the push is triggered too.
   */
  bool enable_batch_recording = false;
  /**
   * @brief The time duration to push metrics when enable_batch_recording is
   * true.
   */
  std::chrono::milliseconds batch_recording_time_duration =
      std::chrono::milliseconds(30000);
};

/**
 * @brief Interface responsible for recording custom metrics.
 */
class MetricClientProviderInterface {
 public:
  virtual ~MetricClientProviderInterface() = default;

  virtual absl::Status Init() noexcept = 0;

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

class MetricClientProviderFactory {
 public:
  /**
   * @brief Factory to create MetricClientProvider.
   *
   * @return std::unique_ptr<MetricClientProviderInterface> created
   * MetricClientProvider.
   */
  static absl::Nonnull<std::unique_ptr<MetricClientProviderInterface>> Create(
      MetricClientOptions options,
      absl::Nonnull<InstanceClientProviderInterface*> instance_client_provider,
      absl::Nonnull<core::AsyncExecutorInterface*> async_executor,
      absl::Nonnull<core::AsyncExecutorInterface*> io_async_executor);
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_INTERFACE_METRIC_CLIENT_PROVIDER_INTERFACE_H_
