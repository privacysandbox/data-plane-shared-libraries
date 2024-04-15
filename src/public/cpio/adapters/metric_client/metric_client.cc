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
absl::Status MetricClient::Init() noexcept {
  return metric_client_provider_->Init();
}

absl::Status MetricClient::Run() noexcept { return absl::OkStatus(); }

absl::Status MetricClient::Stop() noexcept { return absl::OkStatus(); }

absl::Status MetricClient::PutMetrics(
    AsyncContext<PutMetricsRequest, PutMetricsResponse> context) noexcept {
  return metric_client_provider_->PutMetrics(std::move(context));
}

std::unique_ptr<MetricClientInterface> MetricClientFactory::Create(
    MetricClientOptions options) {
  if (options.region.empty()) {
    options.region = GlobalCpio::GetGlobalCpio().GetRegion();
  }
  return std::make_unique<MetricClient>(MetricClientProviderFactory::Create(
      std::move(options),
      &GlobalCpio::GetGlobalCpio().GetInstanceClientProvider(),
      &GlobalCpio::GetGlobalCpio().GetCpuAsyncExecutor(),
      &GlobalCpio::GetGlobalCpio().GetIoAsyncExecutor()));
}
}  // namespace google::scp::cpio
