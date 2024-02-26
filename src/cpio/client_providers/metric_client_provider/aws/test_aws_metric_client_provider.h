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

#ifndef CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_AWS_TEST_AWS_METRIC_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_AWS_TEST_AWS_METRIC_CLIENT_PROVIDER_H_

#include <memory>
#include <string>
#include <utility>

#include <aws/core/Aws.h>
#include <aws/core/client/ClientConfiguration.h>

#include "src/cpio/client_providers/metric_client_provider/aws/aws_metric_client_provider.h"
#include "src/public/cpio/test/metric_client/test_aws_metric_client_options.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc AwsMetricClientProvider
 */
class TestAwsMetricClientProvider : public AwsMetricClientProvider {
 public:
  explicit TestAwsMetricClientProvider(
      TestAwsMetricClientOptions metric_client_options,
      InstanceClientProviderInterface* instance_client_provider,
      core::AsyncExecutorInterface* async_executor,
      core::AsyncExecutorInterface* io_async_executor,
      MetricBatchingOptions metric_batching_options = MetricBatchingOptions())
      : AwsMetricClientProvider(metric_client_options, instance_client_provider,
                                async_executor, io_async_executor,
                                std::move(metric_batching_options)),
        cloud_watch_endpoint_override_(
            std::move(metric_client_options.cloud_watch_endpoint_override)) {}

 protected:
  void CreateClientConfiguration(
      std::string_view region,
      Aws::Client::ClientConfiguration& client_config) noexcept override;

  std::shared_ptr<const std::string> cloud_watch_endpoint_override_;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_AWS_TEST_AWS_METRIC_CLIENT_PROVIDER_H_
