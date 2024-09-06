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

#include "src/public/cpio/adapters/metric_client/metric_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "src/core/interface/errors.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"
#include "src/public/cpio/adapters/metric_client/mock_metric_client_with_overrides.h"
#include "src/public/cpio/interface/metric_client/metric_client_interface.h"
#include "src/public/cpio/interface/metric_client/type_def.h"
#include "src/public/cpio/proto/metric_service/v1/metric_service.pb.h"

using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::cpio::MetricClient;
using google::scp::cpio::MetricClientOptions;
using google::scp::cpio::mock::MockMetricClientWithOverrides;
using testing::Return;

namespace google::scp::cpio::test {
class MetricClientTest : public ::testing::Test {
 protected:
  MetricClientTest() {
    EXPECT_TRUE(client_.Init().ok());
    EXPECT_TRUE(client_.Run().ok());
  }

  ~MetricClientTest() { EXPECT_TRUE(client_.Stop().ok()); }

  MockMetricClientWithOverrides client_;
};

TEST_F(MetricClientTest, PutMetricsSuccess) {
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context;
  EXPECT_CALL(client_.GetMetricClientProvider(), PutMetrics)
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_TRUE(client_.PutMetrics(context).ok());
}
}  // namespace google::scp::cpio::test
