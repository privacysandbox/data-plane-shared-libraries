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

#ifndef PUBLIC_CPIO_ADAPTERS_METRIC_CLIENT_MOCK_MOCK_METRIC_CLIENT_WITH_OVERRIDES_H_
#define PUBLIC_CPIO_ADAPTERS_METRIC_CLIENT_MOCK_MOCK_METRIC_CLIENT_WITH_OVERRIDES_H_

#include <memory>

#include "scp/cc/core/async_executor/mock/mock_async_executor.h"
#include "scp/cc/core/message_router/src/message_router.h"
#include "scp/cc/public/core/interface/execution_result.h"
#include "scp/cc/public/cpio/mock/metric_client/mock_metric_client.h"

namespace google::scp::cpio::mock {
class MockMetricClientWithOverrides : public MetricClient {
 public:
  MockMetricClientWithOverrides(
      const std::shared_ptr<MetricClientOptions>& options)
      : MetricClient(options) {}

  core::ExecutionResult create_metric_client_provider_result =
      core::SuccessExecutionResult();

  core::ExecutionResult CreateMetricClientProvider() noexcept override {
    if (create_metric_client_provider_result.Successful()) {
      metric_client_provider_ = std::make_unique<MockMetricClient>();
      return create_metric_client_provider_result;
    }
    return create_metric_client_provider_result;
  }

  MockMetricClient& GetMetricClientProvider() {
    return dynamic_cast<MockMetricClient&>(*metric_client_provider_);
  }
};
}  // namespace google::scp::cpio::mock

#endif  // PUBLIC_CPIO_ADAPTERS_METRIC_CLIENT_MOCK_MOCK_METRIC_CLIENT_WITH_OVERRIDES_H_
