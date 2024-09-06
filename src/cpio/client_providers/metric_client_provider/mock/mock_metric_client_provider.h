/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_MOCK_MOCK_METRIC_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_MOCK_MOCK_METRIC_CLIENT_PROVIDER_H_

#include <gmock/gmock.h>

#include <memory>

#include "src/cpio/client_providers/interface/metric_client_provider_interface.h"

namespace google::scp::cpio::client_providers::mock {
class MockMetricClientProvider : public MetricClientProviderInterface {
 public:
  MockMetricClientProvider() {
    ON_CALL(*this, Init).WillByDefault(testing::Return(absl::OkStatus()));
  }

  MOCK_METHOD(absl::Status, Init, (), (override, noexcept));
  MOCK_METHOD(
      absl::Status, PutMetrics,
      ((core::AsyncContext<cmrt::sdk::metric_service::v1::PutMetricsRequest,
                           cmrt::sdk::metric_service::v1::PutMetricsResponse>)),
      (override, noexcept));
};
}  // namespace google::scp::cpio::client_providers::mock

#endif  // CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_MOCK_MOCK_METRIC_CLIENT_PROVIDER_H_
