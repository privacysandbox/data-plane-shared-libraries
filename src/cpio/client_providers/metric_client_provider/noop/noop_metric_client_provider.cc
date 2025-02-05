/*
 * Copyright 2025 Google LLC
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

#include <memory>

#include "absl/base/nullability.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/interface/metric_client_provider_interface.h"
#include "src/public/core/interface/execution_result.h"

using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::cpio::client_providers::MetricClientProviderInterface;

namespace {
class NoopMetricClientProvider : public MetricClientProviderInterface {
 public:
  absl::Status Init() noexcept override { return absl::OkStatus(); }

  absl::Status PutMetrics(AsyncContext<PutMetricsRequest, PutMetricsResponse>
                          /* context */) noexcept override {
    return absl::UnimplementedError("");
  }
};
}  // namespace

namespace google::scp::cpio::client_providers {
absl::Nonnull<std::unique_ptr<MetricClientProviderInterface>>
MetricClientProviderFactory::Create(
    MetricClientOptions /*options*/,
    absl::Nonnull<
        InstanceClientProviderInterface*> /* instance_client_provider */,
    absl::Nonnull<AsyncExecutorInterface*> /* async_executor */,
    absl::Nonnull<AsyncExecutorInterface*> /*io_async_executor*/) {
  return std::make_unique<NoopMetricClientProvider>();
}
}  // namespace google::scp::cpio::client_providers
