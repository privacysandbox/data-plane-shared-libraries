
// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/cpio/client_providers/metric_client_provider/metric_client_provider.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <aws/core/Aws.h>

#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "src/core/async_executor/mock/mock_async_executor.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/metric_client_provider/error_codes.h"
#include "src/cpio/client_providers/metric_client_provider/mock/mock_metric_client_provider_with_overrides.h"
#include "src/cpio/common/aws/error_codes.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"
#include "src/public/cpio/proto/metric_service/v1/metric_service.pb.h"

namespace google::scp::cpio::client_providers::test {
namespace {

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::protobuf::Any;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncOperation;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::
    SC_METRIC_CLIENT_PROVIDER_EXECUTOR_NOT_AVAILABLE;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_IS_NOT_RUNNING;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_METRIC_NOT_SET;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_NAMESPACE_NOT_SET;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::mock::MockMetricClientWithOverrides;

constexpr size_t kMetricsBatchSize = 1000;
constexpr std::string_view kMetricNamespace = "test";

class MetricClientProviderTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    SDKOptions options;
    InitAPI(options);
  }

  static void TearDownTestSuite() {
    SDKOptions options;
    ShutdownAPI(options);
  }

  MetricBatchingOptions CreateMetricBatchingOptions(
      bool enable_batch_recording,
      std::string metric_namespace = std::string{kMetricNamespace}) {
    MetricBatchingOptions options;
    options.metric_namespace = std::move(metric_namespace);
    options.enable_batch_recording = enable_batch_recording;
    return options;
  }

  std::shared_ptr<PutMetricsRequest> CreatePutMetricsRequest(
      std::string_view metric_namespace = "") {
    auto request = std::make_shared<PutMetricsRequest>();
    request->set_metric_namespace(metric_namespace);
    auto metric = request->add_metrics();
    metric->set_name("metric1");
    metric->set_value("123");
    return request;
  }

  MockAsyncExecutor mock_async_executor_;
};

TEST_F(MetricClientProviderTest,
       FailsWhenEnableBatchRecordingWithoutNamespace) {
  auto client = std::make_unique<MockMetricClientWithOverrides>(
      &mock_async_executor_, CreateMetricBatchingOptions(true, ""));
  EXPECT_FALSE(client->Init().ok());
}

TEST_F(MetricClientProviderTest,
       FailsWithoutNamespaceInRequestWhenNoBatchRecording) {
  auto client = std::make_unique<MockMetricClientWithOverrides>(
      &mock_async_executor_, CreateMetricBatchingOptions(false));

  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      std::make_shared<PutMetricsRequest>(),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  ASSERT_TRUE(client->Init().ok());
  EXPECT_FALSE(client->PutMetrics(context).ok());
}

TEST_F(MetricClientProviderTest, FailsWhenNoMetricInRequest) {
  auto client = std::make_unique<MockMetricClientWithOverrides>(
      &mock_async_executor_, CreateMetricBatchingOptions(false));

  auto request = std::make_shared<PutMetricsRequest>();
  request->set_metric_namespace(kMetricNamespace);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      std::move(request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  ASSERT_TRUE(client->Init().ok());
  EXPECT_FALSE(client->PutMetrics(context).ok());
}

TEST_F(MetricClientProviderTest, LaunchScheduleMetricsBatchPushWithRun) {
  auto client = std::make_unique<MockMetricClientWithOverrides>(
      &mock_async_executor_, CreateMetricBatchingOptions(true));

  absl::Notification schedule_for_is_called;
  mock_async_executor_.schedule_for_mock =
      [&](AsyncOperation work, Timestamp timestamp,
          std::function<bool()>& cancellation_callback) {
        schedule_for_is_called.Notify();
        return FailureExecutionResult(SC_UNKNOWN);
      };

  EXPECT_FALSE(client->Init().ok());
  schedule_for_is_called.WaitForNotification();
}

TEST_F(MetricClientProviderTest, RecordMetricWithoutBatch) {
  auto client = std::make_unique<MockMetricClientWithOverrides>(
      &mock_async_executor_, CreateMetricBatchingOptions(false));

  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      CreatePutMetricsRequest(kMetricNamespace),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  absl::BlockingCounter batch_push_called_count(2);
  client->metrics_batch_push_mock =
      [&](const std::shared_ptr<std::vector<core::AsyncContext<
              cmrt::sdk::metric_service::v1::PutMetricsRequest,
              cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&
              metric_requests_vector) noexcept {
        EXPECT_EQ(metric_requests_vector->size(), 1);
        batch_push_called_count.DecrementCount();
        return SuccessExecutionResult();
      };

  ASSERT_TRUE(client->Init().ok());
  EXPECT_TRUE(client->PutMetrics(context).ok());
  EXPECT_EQ(client->GetSizeMetricRequestsVector(), 0);
  EXPECT_TRUE(client->PutMetrics(context).ok());
  EXPECT_EQ(client->GetSizeMetricRequestsVector(), 0);
  batch_push_called_count.Wait();
}

TEST_F(MetricClientProviderTest, RecordMetricWithBatch) {
  auto client = std::make_unique<MockMetricClientWithOverrides>(
      &mock_async_executor_, CreateMetricBatchingOptions(true));

  absl::Mutex schedule_for_is_called_mu;
  bool schedule_for_is_called = false;
  mock_async_executor_.schedule_for_mock =
      [&](AsyncOperation work, Timestamp timestamp,
          std::function<bool()>& cancellation_callback) {
        {
          absl::MutexLock lock(&schedule_for_is_called_mu);
          schedule_for_is_called = true;
        }
        return SuccessExecutionResult();
      };

  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      CreatePutMetricsRequest(),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  absl::Mutex batch_push_called_mu;
  bool batch_push_called = false;
  client->metrics_batch_push_mock =
      [&](const std::shared_ptr<std::vector<core::AsyncContext<
              cmrt::sdk::metric_service::v1::PutMetricsRequest,
              cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&
              metric_requests_vector) noexcept {
        {
          absl::MutexLock lock(&batch_push_called_mu);
          batch_push_called = true;
        }
        EXPECT_EQ(metric_requests_vector->size(), kMetricsBatchSize);
        return SuccessExecutionResult();
      };

  ASSERT_TRUE(client->Init().ok());

  for (int i = 0; i <= 2001; i++) {
    EXPECT_TRUE(client->PutMetrics(context).ok());
  }

  {
    absl::MutexLock lock(&schedule_for_is_called_mu);
    schedule_for_is_called_mu.Await(absl::Condition(&schedule_for_is_called));
  }

  absl::MutexLock lock(&batch_push_called_mu);
  batch_push_called_mu.Await(absl::Condition(&batch_push_called));
}

TEST_F(MetricClientProviderTest, RunMetricsBatchPush) {
  auto client = std::make_unique<MockMetricClientWithOverrides>(
      &mock_async_executor_, CreateMetricBatchingOptions(true));

  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      CreatePutMetricsRequest(),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  absl::Notification schedule_metric_push;
  client->schedule_metric_push_mock = [&] {
    schedule_metric_push.Notify();
    return SuccessExecutionResult();
  };

  absl::Notification batch_push_called_count;
  client->metrics_batch_push_mock =
      [&](const std::shared_ptr<std::vector<core::AsyncContext<
              cmrt::sdk::metric_service::v1::PutMetricsRequest,
              cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&
              metric_requests_vector) noexcept {
        EXPECT_EQ(metric_requests_vector->size(), 2);
        batch_push_called_count.Notify();
        return SuccessExecutionResult();
      };

  ASSERT_TRUE(client->Init().ok());

  EXPECT_TRUE(client->PutMetrics(context).ok());
  EXPECT_TRUE(client->PutMetrics(context).ok());
  EXPECT_EQ(client->GetSizeMetricRequestsVector(), 2);
  {
    absl::MutexLock lock(client->mock_sync_mutex_);
    client->RunMetricsBatchPush();
  }
  EXPECT_EQ(client->GetSizeMetricRequestsVector(), 0);
  batch_push_called_count.WaitForNotification();
  schedule_metric_push.WaitForNotification();
}

TEST_F(MetricClientProviderTest, PutMetricSuccessWithMultipleThreads) {
  auto client = std::make_unique<MockMetricClientWithOverrides>(
      nullptr, CreateMetricBatchingOptions(false));
  ASSERT_TRUE(client->Init().ok());
  auto request = CreatePutMetricsRequest(kMetricNamespace);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      std::move(request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  absl::BlockingCounter batch_push_called_count(100);
  client->metrics_batch_push_mock =
      [&](const std::shared_ptr<
          std::vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>&
              metric_requests_vector) noexcept {
        EXPECT_EQ(metric_requests_vector->size(), 1);
        batch_push_called_count.DecrementCount();
        return SuccessExecutionResult();
      };
  std::vector<std::thread> threads;
  for (auto i = 0; i < 100; ++i) {
    threads.emplace_back(
        [&] { EXPECT_TRUE(client->PutMetrics(context).ok()); });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(client->GetSizeMetricRequestsVector(), 0);
  batch_push_called_count.Wait();
}

}  // namespace
}  // namespace google::scp::cpio::client_providers::test
