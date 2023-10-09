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

#include "public/cpio/utils/metric_aggregation/src/metric_utils.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>

#include "core/interface/async_context.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

using google::cmrt::sdk::metric_service::v1::Metric;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::scp::cpio::MetricUnit;
using std::make_shared;
using std::string;

namespace {
constexpr char kMetricName[] = "FrontEndRequestCount";
constexpr char kMetricValue[] = "1234";
constexpr char kNamespace[] = "PBS";
constexpr char kComponentValue[] = "component_name";
constexpr char kMethodValue[] = "method_name";

}  // namespace

namespace google::scp::cpio {

TEST(MetricUtilsTest, GetPutMetricsRequest) {
  auto metric_info = MetricDefinition(
      kMetricName, MetricUnit::kUnknown, kNamespace,
      MetricLabels(
          {{"Key1", "Value1"}, {"Key2", "Value2"}, {"Key3", "Value3"}}));

  auto record_metric_request = make_shared<PutMetricsRequest>();
  MetricUtils::GetPutMetricsRequest(record_metric_request, metric_info,
                                    kMetricValue);

  EXPECT_EQ(record_metric_request->metric_namespace(), kNamespace);
  EXPECT_EQ(record_metric_request->metrics()[0].name(), kMetricName);
  EXPECT_EQ(record_metric_request->metrics()[0].unit(),
            cmrt::sdk::metric_service::v1::MetricUnit::METRIC_UNIT_UNKNOWN);
  EXPECT_EQ(record_metric_request->metrics()[0].value(), kMetricValue);
  EXPECT_EQ(record_metric_request->metrics()[0].labels().size(), 3);
  EXPECT_TRUE(record_metric_request->metrics()[0]
                  .labels()
                  .find(string("Key1"))
                  ->second == string("Value1"));
}

TEST(MetricUtilsTest, CreateMetricLabelsWithComponentSignature) {
  auto metric_labels1 = MetricUtils::CreateMetricLabelsWithComponentSignature(
      kComponentValue, kMethodValue);
  EXPECT_EQ(metric_labels1.size(), 2);
  EXPECT_EQ(metric_labels1.find("ComponentName")->second, kComponentValue);
  EXPECT_EQ(metric_labels1.find("MethodName")->second, kMethodValue);

  auto metric_labels2 =
      MetricUtils::CreateMetricLabelsWithComponentSignature(kComponentValue);
  EXPECT_EQ(metric_labels2.size(), 1);
  EXPECT_EQ(metric_labels2.find("ComponentName")->second, kComponentValue);
}

}  // namespace google::scp::cpio
