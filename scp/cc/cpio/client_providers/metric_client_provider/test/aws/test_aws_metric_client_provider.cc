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

#include "test_aws_metric_client_provider.h"

#include <memory>
#include <string>
#include <utility>

#include <aws/core/Aws.h>
#include <aws/core/client/ClientConfiguration.h>

#include "scp/cc/cpio/client_providers/interface/metric_client_provider_interface.h"
#include "scp/cc/cpio/common/test/aws/test_aws_utils.h"
#include "scp/cc/public/cpio/interface/metric_client/metric_client_interface.h"
#include "scp/cc/public/cpio/test/metric_client/test_aws_metric_client_options.h"

using Aws::Client::ClientConfiguration;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::common::test::CreateTestClientConfiguration;

namespace google::scp::cpio::client_providers {
void TestAwsMetricClientProvider::CreateClientConfiguration(
    const std::string& region, ClientConfiguration& client_config) noexcept {
  client_config =
      CreateTestClientConfiguration(*cloud_watch_endpoint_override_, region);
}

std::unique_ptr<MetricClientInterface> MetricClientProviderFactory::Create(
    MetricClientOptions options,
    InstanceClientProviderInterface* instance_client_provider,
    AsyncExecutorInterface* async_executor,
    core::AsyncExecutorInterface* io_async_executor) {
  return std::make_unique<TestAwsMetricClientProvider>(
      std::move(dynamic_cast<TestAwsMetricClientOptions&>(options)),
      instance_client_provider, async_executor, io_async_executor);
}
}  // namespace google::scp::cpio::client_providers
