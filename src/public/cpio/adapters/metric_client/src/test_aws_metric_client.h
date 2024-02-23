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

#ifndef PUBLIC_CPIO_ADAPTERS_METRIC_CLIENT_TEST_TEST_AWS_METRIC_CLIENT_H_
#define PUBLIC_CPIO_ADAPTERS_METRIC_CLIENT_TEST_TEST_AWS_METRIC_CLIENT_H_

#include <memory>

#include "src/cpio/client_providers/metric_client_provider/src/aws/test_aws_metric_client_provider.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/adapters/metric_client/src/metric_client.h"
#include "src/public/cpio/test/metric_client/test_aws_metric_client_options.h"

namespace google::scp::cpio {
/*! @copydoc MetricClientInterface
 */
class TestAwsMetricClient : public MetricClient {
 public:
  explicit TestAwsMetricClient(
      const std::shared_ptr<TestAwsMetricClientOptions>& options)
      : MetricClient(options) {}
};
}  // namespace google::scp::cpio

#endif  // PUBLIC_CPIO_ADAPTERS_METRIC_CLIENT_TEST_TEST_AWS_METRIC_CLIENT_H_
