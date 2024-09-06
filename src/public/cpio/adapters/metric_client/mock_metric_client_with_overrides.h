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

#ifndef PUBLIC_CPIO_ADAPTERS_METRIC_CLIENT_MOCK_METRIC_CLIENT_WITH_OVERRIDES_H_
#define PUBLIC_CPIO_ADAPTERS_METRIC_CLIENT_MOCK_METRIC_CLIENT_WITH_OVERRIDES_H_

#include <gtest/gtest.h>

#include <memory>

#include "src/cpio/client_providers/metric_client_provider/mock/mock_metric_client_provider.h"

namespace google::scp::cpio::mock {
class MockMetricClientWithOverrides : public MetricClient {
 public:
  MockMetricClientWithOverrides()
      : MetricClient(std::make_unique<testing::NiceMock<
                         client_providers::mock::MockMetricClientProvider>>()) {
  }

  client_providers::mock::MockMetricClientProvider& GetMetricClientProvider() {
    return dynamic_cast<client_providers::mock::MockMetricClientProvider&>(
        *metric_client_provider_);
  }
};
}  // namespace google::scp::cpio::mock

#endif  // PUBLIC_CPIO_ADAPTERS_METRIC_CLIENT_MOCK_METRIC_CLIENT_WITH_OVERRIDES_H_
