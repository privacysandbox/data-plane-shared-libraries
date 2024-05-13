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

#ifndef CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_GCP_GCP_METRIC_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_GCP_GCP_METRIC_CLIENT_PROVIDER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "google/cloud/future.h"
#include "google/cloud/monitoring/metric_client.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/instance_client_provider/gcp/gcp_instance_client_utils.h"
#include "src/cpio/client_providers/metric_client_provider/gcp/error_codes.h"
#include "src/cpio/client_providers/metric_client_provider/metric_client_provider.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/metric_service/v1/metric_service.pb.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc MetricClientProvider
 */
class GcpMetricClientProvider : public MetricClientProvider {
 public:
  explicit GcpMetricClientProvider(
      absl::Nonnull<InstanceClientProviderInterface*> instance_client_provider,
      absl::Nonnull<core::AsyncExecutorInterface*> async_executor,
      MetricBatchingOptions metric_batching_options = MetricBatchingOptions())
      : MetricClientProvider(async_executor,
                             std::move(metric_batching_options)),
        instance_client_provider_(instance_client_provider) {}

  GcpMetricClientProvider() = delete;

  absl::Status Init() noexcept override;

 protected:
  explicit GcpMetricClientProvider(
      std::shared_ptr<google::cloud::monitoring::MetricServiceClient>
          metric_service_client,
      absl::Nonnull<InstanceClientProviderInterface*> instance_client_provider,
      absl::Nonnull<core::AsyncExecutorInterface*> async_executor,
      MetricBatchingOptions metric_batching_options = MetricBatchingOptions())
      : MetricClientProvider(async_executor,
                             std::move(metric_batching_options)),
        instance_client_provider_(instance_client_provider),
        metric_service_client_(std::move(metric_service_client)) {}

  virtual void CreateMetricServiceClient() noexcept;

  core::ExecutionResult MetricsBatchPush(
      const std::shared_ptr<std::vector<core::AsyncContext<
          cmrt::sdk::metric_service::v1::PutMetricsRequest,
          cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&
          metric_requests_vector) noexcept override
      ABSL_LOCKS_EXCLUDED(sync_mutex_);

  /**
   * @brief Is called after GCP AsyncCreateTimeSeries is completed.
   *
   * @param record_metric_context the record custom metric operation
   * context.
   * @param outcome the operation outcome of GCP AsyncCreateTimeSeries.
   */
  virtual void OnAsyncCreateTimeSeriesCallback(
      std::vector<
          core::AsyncContext<cmrt::sdk::metric_service::v1::PutMetricsRequest,
                             cmrt::sdk::metric_service::v1::PutMetricsResponse>>
          metric_requests_vector,
      google::cloud::future<google::cloud::Status> outcome) noexcept
      ABSL_LOCKS_EXCLUDED(sync_mutex_);

 private:
  /// Instance client provider to fetch cloud metadata.
  InstanceClientProviderInterface* instance_client_provider_;

  GcpInstanceResourceNameDetails instance_resource_;

  /// An Instance of the Gcp metric service client.
  std::shared_ptr<const google::cloud::monitoring::MetricServiceClient>
      metric_service_client_;
};

}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_GCP_GCP_METRIC_CLIENT_PROVIDER_H_
