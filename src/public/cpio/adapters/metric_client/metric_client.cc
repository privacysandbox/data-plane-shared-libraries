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

#include "metric_client.h"

#include <memory>
#include <utility>

#include <google/protobuf/util/time_util.h>

#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/public/cpio/proto/metric_service/v1/metric_service.pb.h"

using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::scp::core::AsyncContext;
using google::scp::cpio::MetricClientInterface;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::MetricClientProviderFactory;

namespace google::scp::cpio {
<<<<<<< HEAD
void MetricClient::CreateMetricClientProvider() noexcept {
  cpio_ = &GlobalCpio::GetGlobalCpio();
  metric_client_provider_ = MetricClientProviderFactory::Create(
      options_, &cpio_->GetInstanceClientProvider(),
      &cpio_->GetCpuAsyncExecutor(), &cpio_->GetIoAsyncExecutor());
}

ExecutionResult MetricClient::Init() noexcept {
  CreateMetricClientProvider();
  if (absl::Status error = metric_client_provider_->Init(); !error.ok()) {
    SCP_ERROR(kMetricClient, kZeroUuid, error,
              "Failed to initialize MetricClient.");
    return FailureExecutionResult(SC_UNKNOWN);
  }
  return SuccessExecutionResult();
}

ExecutionResult MetricClient::Run() noexcept {
  if (absl::Status error = metric_client_provider_->Run(); !error.ok()) {
    SCP_ERROR(kMetricClient, kZeroUuid, error, "Failed to run MetricClient.");
    return FailureExecutionResult(SC_UNKNOWN);
  }
  return SuccessExecutionResult();
}

ExecutionResult MetricClient::Stop() noexcept {
  if (absl::Status error = metric_client_provider_->Stop(); !error.ok()) {
    SCP_ERROR(kMetricClient, kZeroUuid, error, "Failed to stop MetricClient.");
    return FailureExecutionResult(SC_UNKNOWN);
  }
  return SuccessExecutionResult();
}

core::ExecutionResult MetricClient::PutMetrics(
=======
absl::Status MetricClient::Init() noexcept {
  return metric_client_provider_->Init();
}

absl::Status MetricClient::Run() noexcept { return absl::OkStatus(); }

absl::Status MetricClient::Stop() noexcept { return absl::OkStatus(); }

absl::Status MetricClient::PutMetrics(
>>>>>>> upstream-3e92e75-3.10.0
    AsyncContext<PutMetricsRequest, PutMetricsResponse> context) noexcept {
  if (!metric_client_provider_->PutMetrics(std::move(context)).ok()) {
    return FailureExecutionResult(SC_UNKNOWN);
  }
  return SuccessExecutionResult();
}

std::unique_ptr<MetricClientInterface> MetricClientFactory::Create(
    MetricClientOptions options) {
<<<<<<< HEAD
  return std::make_unique<MetricClient>(std::move(options));
=======
  if (options.region.empty()) {
    options.region = GlobalCpio::GetGlobalCpio().GetRegion();
  }
  return std::make_unique<MetricClient>(MetricClientProviderFactory::Create(
      std::move(options),
      &GlobalCpio::GetGlobalCpio().GetInstanceClientProvider(),
      &GlobalCpio::GetGlobalCpio().GetCpuAsyncExecutor(),
      &GlobalCpio::GetGlobalCpio().GetIoAsyncExecutor()));
>>>>>>> upstream-3e92e75-3.10.0
}
}  // namespace google::scp::cpio
