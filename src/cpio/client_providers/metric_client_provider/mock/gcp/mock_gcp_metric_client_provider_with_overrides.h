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

#ifndef CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_MOCK_GCP_MOCK_GCP_METRIC_CLIENT_PROVIDER_WITH_OVERRIDES_H_
#define CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_MOCK_GCP_MOCK_GCP_METRIC_CLIENT_PROVIDER_WITH_OVERRIDES_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "google/protobuf/any.pb.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/metric_client_provider/gcp/gcp_metric_client_provider.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers::mock {

class MockGcpMetricClientProviderOverrides : public GcpMetricClientProvider {
 public:
  explicit MockGcpMetricClientProviderOverrides(
      std::shared_ptr<google::cloud::monitoring::MetricServiceClient>
          metric_service_client,
      MetricBatchingOptions metric_batching_options,
      absl::Nonnull<InstanceClientProviderInterface*> instance_client_provider,
      absl::Nonnull<core::AsyncExecutorInterface*> async_executor)
      : GcpMetricClientProvider(std::move(metric_service_client),
                                instance_client_provider, async_executor,
                                std::move(metric_batching_options)) {}

  core::ExecutionResult MetricsBatchPush(
      const std::shared_ptr<std::vector<core::AsyncContext<
          cmrt::sdk::metric_service::v1::PutMetricsRequest,
          cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&
          metric_requests_vector) noexcept override {
    return GcpMetricClientProvider::MetricsBatchPush(metric_requests_vector);
  }

  void OnAsyncCreateTimeSeriesCallback(
      std::vector<
          core::AsyncContext<cmrt::sdk::metric_service::v1::PutMetricsRequest,
                             cmrt::sdk::metric_service::v1::PutMetricsResponse>>
          metric_requests_vector,
      google::cloud::future<google::cloud::Status> outcome) noexcept override {
    return GcpMetricClientProvider::OnAsyncCreateTimeSeriesCallback(
        metric_requests_vector, std::move(outcome));
  }

  void CreateMetricServiceClient() noexcept override {}
};
}  // namespace google::scp::cpio::client_providers::mock

#endif  // CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_MOCK_GCP_MOCK_GCP_METRIC_CLIENT_PROVIDER_WITH_OVERRIDES_H_
