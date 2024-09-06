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

#include "src/cpio/client_providers/metric_client_provider/gcp/gcp_metric_client_provider.h"

#include <gtest/gtest.h>

#include <memory>
#include <string_view>
#include <vector>

#include <google/protobuf/util/time_util.h>

#include "absl/strings/numbers.h"
#include "google/cloud/monitoring/mocks/mock_metric_connection.h"
#include "src/core/async_executor/mock/mock_async_executor.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "src/cpio/client_providers/interface/metric_client_provider_interface.h"
#include "src/cpio/client_providers/metric_client_provider/gcp/error_codes.h"
#include "src/cpio/client_providers/metric_client_provider/gcp/gcp_metric_client_utils.h"
#include "src/cpio/client_providers/metric_client_provider/mock/gcp/mock_gcp_metric_client_provider_with_overrides.h"
#include "src/cpio/common/gcp/error_codes.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"
#include "src/public/cpio/interface/metric_client/metric_client_interface.h"
#include "src/public/cpio/interface/metric_client/type_def.h"
#include "src/public/cpio/proto/metric_service/v1/metric_service.pb.h"

using google::cloud::make_ready_future;
using google::cloud::Status;
using google::cloud::StatusCode;
using google::cloud::monitoring::MetricServiceClient;
using google::cloud::monitoring_mocks::MockMetricServiceConnection;
using google::cmrt::sdk::metric_service::v1::Metric;
using google::cmrt::sdk::metric_service::v1::MetricUnit;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::monitoring::v3::CreateTimeSeriesRequest;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_GCP_INVALID_ARGUMENT;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::GcpMetricClientUtils;
using google::scp::cpio::client_providers::mock::
    MockGcpMetricClientProviderOverrides;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using testing::ByMove;
using testing::NiceMock;
using testing::Return;

namespace google::scp::cpio::client_providers::gcp_metric_client::test {
namespace {
constexpr std::string_view kName = "test_name";
constexpr std::string_view kValue = "12346.89";
constexpr std::string_view kNamespace = "gcp_namespace";
constexpr std::string_view kDifferentNamespace = "different_namespace";
constexpr std::string_view kProjectIdValue = "123456789";
constexpr std::string_view kInstanceIdValue = "987654321";
constexpr std::string_view kInstanceZoneValue = "us-central1-c";

constexpr std::string_view kInstanceResourceName =
    R"(//compute.googleapis.com/projects/123456789/zones/us-central1-c/instances/987654321)";

constexpr std::string_view kResourceType = "gce_instance";
constexpr std::string_view kProjectIdKey = "project_id";
constexpr std::string_view kInstanceIdKey = "instance_id";
constexpr std::string_view kInstanceZoneKey = "zone";

class GcpMetricClientProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    async_executor_mock_.schedule_for_mock =
        [](core::AsyncOperation work, Timestamp timestamp,
           std::function<bool()>& cancellation_callback) -> ExecutionResult {
      return core::SuccessExecutionResult();
    };

    instance_client_provider_mock_.instance_resource_name =
        kInstanceResourceName;

    connection_ = std::make_shared<NiceMock<MockMetricServiceConnection>>();
    mock_client_ = std::make_shared<MetricServiceClient>(connection_);

    metric_client_provider_ = CreateClient(false);
    ASSERT_TRUE(metric_client_provider_->Init().ok());
  }

  std::unique_ptr<MockGcpMetricClientProviderOverrides> CreateClient(
      bool enable_batch_recording) {
    MetricBatchingOptions metric_batching_options;
    metric_batching_options.enable_batch_recording = enable_batch_recording;
    if (enable_batch_recording) {
      metric_batching_options.metric_namespace = kNamespace;
    }

    return std::make_unique<MockGcpMetricClientProviderOverrides>(
        mock_client_, std::move(metric_batching_options),
        &instance_client_provider_mock_, &async_executor_mock_);
  }

  MockAsyncExecutor async_executor_mock_;
  MockInstanceClientProvider instance_client_provider_mock_;
  std::shared_ptr<MetricServiceClient> mock_client_;
  std::shared_ptr<MockMetricServiceConnection> connection_;
  std::unique_ptr<MockGcpMetricClientProviderOverrides> metric_client_provider_;
};

static void SetPutMetricsRequest(
    PutMetricsRequest& record_metric_request, std::string_view value = kValue,
    const int64_t& timestamp_in_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count()) {
  auto metric = record_metric_request.add_metrics();
  metric->set_name(kName);
  metric->set_value(value);
  *metric->mutable_timestamp() =
      TimeUtil::MillisecondsToTimestamp(timestamp_in_ms);
}

MATCHER_P2(RequestEquals, metric_name, metric_namespace, "") {
  bool equal = true;
  if (arg.name() != metric_name) {
    equal = false;
  }
  if (arg.time_series()[0].metric().type() !=
      "custom.googleapis.com/" + std::string(metric_namespace) + "/" +
          std::string(kName)) {
    equal = false;
  }
  double value = 0.0;
  if (!absl::SimpleAtod(std::string_view(kValue), &value) ||
      arg.time_series()[0].points()[0].value().double_value() != value) {
    equal = false;
  }
  auto resource = arg.time_series()[0].resource();
  if (resource.type() != kResourceType) {
    equal = false;
  }
  if (resource.labels().find(kProjectIdKey)->second != kProjectIdValue) {
    equal = false;
  }
  if (resource.labels().find(kInstanceIdKey)->second != kInstanceIdValue) {
    equal = false;
  }
  if (resource.labels().find(kInstanceZoneKey)->second != kInstanceZoneValue) {
    equal = false;
  }
  return equal;
}

TEST_F(GcpMetricClientProviderTest,
       UseNamespaceFromRequestWithoutBatchRecording) {
  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request);
  record_metric_request.set_metric_namespace(kDifferentNamespace);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      std::make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});
  auto requests_vector = std::make_shared<
      std::vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>();

  auto metric_name =
      GcpMetricClientUtils::ConstructProjectName(kProjectIdValue);
  std::atomic<int> received_metrics = 0;
  EXPECT_CALL(*connection_, AsyncCreateTimeSeries(RequestEquals(
                                metric_name, kDifferentNamespace)))
      .WillRepeatedly([&](CreateTimeSeriesRequest const& request) {
        received_metrics.fetch_add(request.time_series().size());
        return make_ready_future(Status(StatusCode::kOk, ""));
      });

  for (auto i = 0; i < 5; i++) {
    requests_vector->emplace_back(context);
  }
  auto result = metric_client_provider_->MetricsBatchPush(requests_vector);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(received_metrics, 5);
}

TEST_F(GcpMetricClientProviderTest, MetricsBatchPush) {
  metric_client_provider_ = CreateClient(true);
  ASSERT_TRUE(metric_client_provider_->Init().ok());

  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      std::make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});
  auto requests_vector = std::make_shared<
      std::vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>();

  auto metric_name =
      GcpMetricClientUtils::ConstructProjectName(kProjectIdValue);
  std::atomic<int> received_metrics = 0;
  EXPECT_CALL(*connection_,
              AsyncCreateTimeSeries(RequestEquals(metric_name, kNamespace)))
      .WillRepeatedly([&](CreateTimeSeriesRequest const& request) {
        received_metrics.fetch_add(request.time_series().size());
        return make_ready_future(Status(StatusCode::kOk, ""));
      });

  for (auto i = 0; i < 5; i++) {
    requests_vector->emplace_back(context);
  }
  auto result = metric_client_provider_->MetricsBatchPush(requests_vector);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(received_metrics, 5);
}

TEST_F(GcpMetricClientProviderTest, FailedMetricsBatchPush) {
  metric_client_provider_ = CreateClient(true);
  ASSERT_TRUE(metric_client_provider_->Init().ok());

  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request);
  std::atomic<int> metric_responses = 0;
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      std::make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {
        metric_responses++;
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_GCP_INVALID_ARGUMENT)));
      });
  auto requests_vector = std::make_shared<
      std::vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>();

  auto metric_name =
      GcpMetricClientUtils::ConstructProjectName(kProjectIdValue);
  std::atomic<int> received_metrics = 0;
  EXPECT_CALL(*connection_,
              AsyncCreateTimeSeries(RequestEquals(metric_name, kNamespace)))
      .WillRepeatedly([&](CreateTimeSeriesRequest const& request) {
        received_metrics.fetch_add(request.time_series().size());
        return make_ready_future(
            Status(StatusCode::kInvalidArgument, "Error Not Found"));
      });

  for (auto i = 0; i < 5; i++) {
    requests_vector->emplace_back(context);
  }
  auto result = metric_client_provider_->MetricsBatchPush(requests_vector);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(metric_responses, 5);
  EXPECT_EQ(received_metrics, 5);
}
}  // namespace
}  // namespace google::scp::cpio::client_providers::gcp_metric_client::test
