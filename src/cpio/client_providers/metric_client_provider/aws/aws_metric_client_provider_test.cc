// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/cpio/client_providers/metric_client_provider/aws/aws_metric_client_provider.h"

#include <gtest/gtest.h>

#include <aws/core/Aws.h>

#include "absl/cleanup/cleanup.h"
#include "src/core/async_executor/mock/mock_async_executor.h"
#include "src/cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"

using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;

namespace google::scp::cpio::client_providers {
namespace {
TEST(AwsMetricClientProviderTest, InitGetsRegionFromInstanceClient) {
  Aws::InitAPI(/*options=*/{});
  absl::Cleanup cleanup([] { Aws::ShutdownAPI(/*options=*/{}); });
  MockAsyncExecutor async_executor_mock;
  MockAsyncExecutor io_async_executor_mock;
  MockInstanceClientProvider instance_client_mock;

  // Non-empty region set.
  AwsMetricClientProvider client(MetricClientOptions{}, &instance_client_mock,
                                 &async_executor_mock, &io_async_executor_mock);
  ASSERT_TRUE(client.Init().ok());
}

TEST(AwsMetricClientProviderTest, InitGetsRegionFromOptions) {
  Aws::InitAPI(/*options=*/{});
  absl::Cleanup cleanup([] { Aws::ShutdownAPI(/*options=*/{}); });
  MockAsyncExecutor async_executor_mock;
  MockAsyncExecutor io_async_executor_mock;
  MockInstanceClientProvider instance_client_mock;
  instance_client_mock.get_instance_resource_name_mock = absl::UnknownError("");

  // Non-empty region set.
  AwsMetricClientProvider client(MetricClientOptions{.region = "foo"},
                                 &instance_client_mock, &async_executor_mock,
                                 &io_async_executor_mock);
  ASSERT_TRUE(client.Init().ok());
}

TEST(AwsMetricClientProviderTest, InitFailedToFetchRegion) {
  MockAsyncExecutor async_executor_mock;
  MockAsyncExecutor io_async_executor_mock;
  MockInstanceClientProvider instance_client_mock;
  instance_client_mock.get_instance_resource_name_mock = absl::UnknownError("");

  // Empty region set.
  AwsMetricClientProvider client(MetricClientOptions{}, &instance_client_mock,
                                 &async_executor_mock, &io_async_executor_mock);
  ASSERT_FALSE(client.Init().ok());
}
}  // namespace
}  // namespace google::scp::cpio::client_providers
