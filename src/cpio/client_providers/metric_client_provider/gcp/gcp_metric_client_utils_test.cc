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

#include "src/cpio/client_providers/metric_client_provider/gcp/gcp_metric_client_utils.h"

#include <gtest/gtest.h>

#include <memory>
#include <string_view>

#include <google/protobuf/util/time_util.h>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/numbers.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/metric_client_provider/gcp/error_codes.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"
#include "src/public/cpio/proto/metric_service/v1/metric_service.pb.h"

using google::api::MonitoredResource;
using google::cloud::Status;
using google::cloud::StatusCode;
using google::cmrt::sdk::metric_service::v1::Metric;
using google::cmrt::sdk::metric_service::v1::MetricUnit;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::monitoring::v3::Point;
using google::monitoring::v3::TimeSeries;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::
    SC_GCP_METRIC_CLIENT_FAILED_OVERSIZE_METRIC_LABELS;
using google::scp::core::errors::
    SC_GCP_METRIC_CLIENT_FAILED_WITH_INVALID_TIMESTAMP;
using google::scp::core::errors::SC_GCP_METRIC_CLIENT_INVALID_METRIC_VALUE;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::GcpMetricClientUtils;

namespace google::scp::cpio::test {
namespace {
constexpr std::string_view kName = "test_name";
constexpr std::string_view kValue = "12346.89";
constexpr std::string_view kBadValue = "ab33c6";
constexpr MetricUnit kUnit = MetricUnit::METRIC_UNIT_COUNT;
constexpr std::string_view kNamespace = "test_namespace";
constexpr std::string_view kMetricTypePrefix = "custom.googleapis.com";
constexpr std::string_view kProjectIdValue = "project_id_test";
constexpr std::string_view kInstanceIdValue = "instance_id_test";
constexpr std::string_view kInstanceZoneValue = "zone_test";
constexpr std::string_view kResourceType = "gce_instance";
constexpr std::string_view kProjectIdKey = "project_id";
constexpr std::string_view kInstanceIdKey = "instance_id";
constexpr std::string_view kInstanceZoneKey = "zone";

class GcpMetricClientUtilsTest : public ::testing::Test {
 protected:
  void SetPutMetricsRequest(
      PutMetricsRequest& record_metric_request, std::string_view value = kValue,
      const int64_t& timestamp_in_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count()) {
    auto metric = record_metric_request.add_metrics();
    metric->set_name(kName);
    metric->set_value(value);
    metric->set_unit(kUnit);
    *metric->mutable_timestamp() =
        TimeUtil::MillisecondsToTimestamp(timestamp_in_ms);
    const absl::flat_hash_map<std::string, std::string> labels{
        {"CPU", "10"},
        {"GPU", "15"},
        {"RAM", "20"},
    };
    metric->mutable_labels()->insert(labels.begin(), labels.end());
  }
};

TEST_F(GcpMetricClientUtilsTest, ParseRequestToTimeSeries) {
  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      std::make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  std::vector<TimeSeries> time_series_list;

  auto result = GcpMetricClientUtils::ParseRequestToTimeSeries(
      context, kNamespace, time_series_list);
  auto expected_type = std::string(kMetricTypePrefix) + "/" +
                       std::string(kNamespace) + "/test_name";
  auto expected_timestamp = context.request->metrics()[0].timestamp();

  auto time_series = time_series_list[0];
  double value = 0.0;
  EXPECT_TRUE(absl::SimpleAtod(std::string_view(kValue), &value));
  ASSERT_SUCCESS(result);
  EXPECT_EQ(time_series.metric().type(), expected_type);
  EXPECT_EQ(time_series.unit(), "");
  EXPECT_EQ(time_series.metric().labels().size(), 3);
  EXPECT_EQ(time_series.points()[0].value().double_value(), value);
  EXPECT_EQ(time_series.points()[0].interval().end_time(), expected_timestamp);
}

TEST_F(GcpMetricClientUtilsTest, FailedWithBadMetricValue) {
  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request, kBadValue);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      std::make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  std::vector<TimeSeries> time_series_list;

  auto result = GcpMetricClientUtils::ParseRequestToTimeSeries(
      context, kNamespace, time_series_list);
  auto expected_type = std::string(kMetricTypePrefix) + "/" +
                       std::string(kNamespace) + "/test_name";
  auto expected_timestamp = context.request->metrics()[0].timestamp();

  auto time_series = time_series_list[0];
  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          SC_GCP_METRIC_CLIENT_INVALID_METRIC_VALUE)));
}

TEST_F(GcpMetricClientUtilsTest, BadTimeStamp) {
  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request, kValue, -123);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      std::make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  std::vector<TimeSeries> time_series_list;

  auto result = GcpMetricClientUtils::ParseRequestToTimeSeries(
      context, kNamespace, time_series_list);
  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          SC_GCP_METRIC_CLIENT_FAILED_WITH_INVALID_TIMESTAMP)));
}

TEST_F(GcpMetricClientUtilsTest, InvalidTimeStamp) {
  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request, kValue, 12345);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      std::make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  std::vector<TimeSeries> time_series_list;

  auto result = GcpMetricClientUtils::ParseRequestToTimeSeries(
      context, kNamespace, time_series_list);
  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          SC_GCP_METRIC_CLIENT_FAILED_WITH_INVALID_TIMESTAMP)));
}

TEST_F(GcpMetricClientUtilsTest, OverSizeLabels) {
  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      std::make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});

  // Adds oversize labels.
  auto& labels = *context.request->add_metrics()->mutable_labels();
  for (int i = 0; i < 33; ++i) {
    labels[absl::StrCat("key", i)] = "value";
  }
  std::vector<TimeSeries> time_series_list;
  auto result = GcpMetricClientUtils::ParseRequestToTimeSeries(
      context, kNamespace, time_series_list);
  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          SC_GCP_METRIC_CLIENT_FAILED_OVERSIZE_METRIC_LABELS)));
}

TEST(GcpMetricClientUtilsTestII, AddResourceToTimeSeries) {
  std::vector<TimeSeries> time_series_list(10);

  GcpMetricClientUtils::AddResourceToTimeSeries(
      kProjectIdValue, kInstanceIdValue, kInstanceZoneValue, time_series_list);

  for (auto time_series : time_series_list) {
    auto resource = time_series.resource();
    EXPECT_EQ(resource.type(), kResourceType);
    EXPECT_EQ(resource.labels().find(kProjectIdKey)->second, kProjectIdValue);
    EXPECT_EQ(resource.labels().find(kInstanceIdKey)->second, kInstanceIdValue);
    EXPECT_EQ(resource.labels().find(kInstanceZoneKey)->second,
              kInstanceZoneValue);
  }
}
}  // namespace
}  // namespace google::scp::cpio::test
