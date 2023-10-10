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

#include "public/cpio/utils/metric_aggregation/src/simple_metric.h"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/async_executor/mock/mock_async_executor.h"
#include "core/interface/async_context.h"
#include "core/message_router/src/error_codes.h"
#include "core/message_router/src/message_router.h"
#include "core/test/utils/auto_init_run_stop.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/mock/metric_client/mock_metric_client.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"
#include "public/cpio/utils/metric_aggregation/interface/type_def.h"

using google::cmrt::sdk::metric_service::v1::Metric;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AsyncOperation;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::test::AutoInitRunStop;
using google::scp::core::test::WaitUntil;
using google::scp::cpio::MetricUnit;
using google::scp::cpio::MockMetricClient;
using std::make_shared;
using std::shared_ptr;
using std::static_pointer_cast;
using std::vector;

namespace {
constexpr char kMetricName[] = "FrontEndRequestCount";
constexpr char kMetricNameUpdate[] = "NewMetricName";
constexpr char kMetricValue[] = "1234";
constexpr char kNamespace[] = "PBS";

}  // namespace

namespace google::scp::cpio {

class SimpleMetricTest : public testing::Test {
 protected:
  SimpleMetricTest() {
    mock_metric_client_ = make_shared<MockMetricClient>();
    auto mock_async_executor_ = make_shared<MockAsyncExecutor>();
    auto metric_info =
        MetricDefinition(kMetricName, MetricUnit::kCount, kNamespace);
    simple_metric_ = make_shared<SimpleMetric>(
        mock_async_executor_, mock_metric_client_, std::move(metric_info));

    AutoInitRunStop to_handle_simple_metric(*simple_metric_);
  }

  shared_ptr<MockMetricClient> mock_metric_client_;
  shared_ptr<SimpleMetricInterface> simple_metric_;
};

TEST_F(SimpleMetricTest, Push) {
  Metric metric_received;
  bool schedule_is_called = false;
  EXPECT_CALL(*mock_metric_client_, PutMetrics).WillOnce([&](auto context) {
    schedule_is_called = true;
    metric_received.CopyFrom(context.request->metrics()[0]);
    context.result = FailureExecutionResult(123);
    context.Finish();
    return context.result;
  });

  simple_metric_->Push(kMetricValue);
  WaitUntil([&]() { return schedule_is_called; });

  EXPECT_EQ(metric_received.name(), kMetricName);
  EXPECT_EQ(metric_received.unit(),
            cmrt::sdk::metric_service::v1::MetricUnit::METRIC_UNIT_COUNT);
  EXPECT_EQ(metric_received.value(), kMetricValue);
}

TEST_F(SimpleMetricTest, PushWithMetricInfo) {
  Metric metric_received;
  bool schedule_is_called = false;
  EXPECT_CALL(*mock_metric_client_, PutMetrics).WillOnce([&](auto context) {
    schedule_is_called = true;
    metric_received.CopyFrom(context.request->metrics()[0]);
    context.result = FailureExecutionResult(123);
    context.Finish();
    return context.result;
  });

  auto metric_info_updated = MetricDefinition(
      kMetricNameUpdate, MetricUnit::kMilliseconds, kNamespace);
  simple_metric_->Push(kMetricValue, metric_info_updated);
  WaitUntil([&]() { return schedule_is_called; });

  EXPECT_EQ(metric_received.name(), kMetricNameUpdate);
  EXPECT_EQ(
      metric_received.unit(),
      cmrt::sdk::metric_service::v1::MetricUnit::METRIC_UNIT_MILLISECONDS);
  EXPECT_EQ(metric_received.value(), kMetricValue);
}

}  // namespace google::scp::cpio
