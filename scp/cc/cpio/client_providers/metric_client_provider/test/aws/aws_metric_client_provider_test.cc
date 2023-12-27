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

#include "cpio/client_providers/metric_client_provider/src/aws/aws_metric_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <aws/core/Aws.h>
#include <aws/monitoring/CloudWatchClient.h>
#include <aws/monitoring/CloudWatchErrors.h>
#include <aws/monitoring/model/PutMetricDataRequest.h>
#include <google/protobuf/util/time_util.h>

#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/metric_client_provider/mock/aws/mock_aws_metric_client_provider_with_overrides.h"
#include "cpio/client_providers/metric_client_provider/mock/aws/mock_cloud_watch_client.h"
#include "cpio/client_providers/metric_client_provider/src/aws/aws_metric_client_utils.h"
#include "cpio/client_providers/metric_client_provider/src/aws/error_codes.h"
#include "cpio/common/src/aws/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::Client::AWSError;
using Aws::CloudWatch::CloudWatchErrors;
using Aws::CloudWatch::Model::MetricDatum;
using Aws::CloudWatch::Model::PutMetricDataOutcome;
using Aws::CloudWatch::Model::PutMetricDataRequest;
using google::cmrt::sdk::metric_service::v1::MetricUnit;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::Timestamp;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::
    SC_AWS_METRIC_CLIENT_PROVIDER_SHOULD_ENABLE_BATCH_RECORDING;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::AwsMetricClientUtils;
using google::scp::cpio::client_providers::mock::
    MockAwsMetricClientProviderOverrides;
using google::scp::cpio::client_providers::mock::MockCloudWatchClient;
using ::testing::StrEq;

namespace {
constexpr char kResourceNameMock[] =
    "arn:aws:ec2:us-east-1:123456789012:instance/i-0e9801d129EXAMPLE";
constexpr char kName[] = "test_name";
constexpr char kValue[] = "12346";
constexpr MetricUnit kUnit = MetricUnit::METRIC_UNIT_COUNT;
constexpr char kNamespace[] = "aws_name_space";
}  // namespace

namespace google::scp::cpio::client_providers::test {
class AwsMetricClientProviderTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    SDKOptions options;
    InitAPI(options);
  }

  static void TearDownTestSuite() {
    SDKOptions options;
    ShutdownAPI(options);
  }

  std::unique_ptr<MockAwsMetricClientProviderOverrides> CreateClient(
      bool enable_batch_recording) {
    auto metric_batching_options = std::make_shared<MetricBatchingOptions>();
    metric_batching_options->enable_batch_recording = enable_batch_recording;
    if (enable_batch_recording) {
      metric_batching_options->metric_namespace = kNamespace;
    }

    return std::make_unique<MockAwsMetricClientProviderOverrides>(
        metric_batching_options);
  }

  void SetPutMetricsRequest(
      PutMetricsRequest& record_metric_request, std::string_view value = kValue,
      int metrics_num = 1,
      const int64_t& timestamp_in_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count()) {
    record_metric_request.set_metric_namespace(kNamespace);
    for (auto i = 0; i < metrics_num; i++) {
      auto metric = record_metric_request.add_metrics();
      metric->set_name(kName);
      metric->set_value(value);
      metric->set_unit(kUnit);
      *metric->mutable_timestamp() =
          TimeUtil::MillisecondsToTimestamp(timestamp_in_ms);
    }
  }
};

TEST_F(AwsMetricClientProviderTest, InitSuccess) {
  auto client = CreateClient(false);
  client->GetInstanceClientProvider()->instance_resource_name =
      kResourceNameMock;
  EXPECT_SUCCESS(client->Init());
  EXPECT_SUCCESS(client->Run());
  EXPECT_SUCCESS(client->Stop());
}

TEST_F(AwsMetricClientProviderTest, FailedToGetRegion) {
  auto client = CreateClient(false);
  auto failure = FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR);
  client->GetInstanceClientProvider()->get_instance_resource_name_mock =
      failure;
  EXPECT_SUCCESS(client->Init());
  EXPECT_THAT(client->Run(), ResultIs(failure));
}

TEST_F(AwsMetricClientProviderTest, SplitsOversizeRequestsVector) {
  auto client = CreateClient(true);

  client->GetInstanceClientProvider()->instance_resource_name =
      kResourceNameMock;
  EXPECT_SUCCESS(client->Init());
  EXPECT_SUCCESS(client->Run());

  Aws::NoResult result;
  client->GetCloudWatchClient()->put_metric_data_outcome_mock =
      PutMetricDataOutcome(result);

  absl::BlockingCounter put_metric_data_request_count(10);
  client->GetCloudWatchClient()->put_metric_data_async_mock =
      [&](const Aws::CloudWatch::Model::PutMetricDataRequest& request,
          const Aws::CloudWatch::PutMetricDataResponseReceivedHandler& handler,
          const std::shared_ptr<const Aws::Client::AsyncCallerContext>&
              context) {
        EXPECT_THAT(request.GetNamespace(), StrEq(kNamespace));
        put_metric_data_request_count.DecrementCount();
        return;
      };

  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      std::make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});
  auto requests_vector = std::make_shared<
      std::vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>();
  PutMetricDataRequest request_mock;
  for (auto i = 0; i < 10000; i++) {
    requests_vector->emplace_back(context);
  }

  EXPECT_SUCCESS(client->MetricsBatchPush(requests_vector));
  put_metric_data_request_count.Wait();

  // Cannot stop the client because the AWS callback is mocked.
}

TEST_F(AwsMetricClientProviderTest, KeepMetricsInTheSameRequest) {
  auto client = CreateClient(true);

  client->GetInstanceClientProvider()->instance_resource_name =
      kResourceNameMock;
  EXPECT_SUCCESS(client->Init());
  EXPECT_SUCCESS(client->Run());

  Aws::NoResult result;
  client->GetCloudWatchClient()->put_metric_data_outcome_mock =
      PutMetricDataOutcome(result);

  absl::BlockingCounter put_metric_data_request_count(3);
  absl::Mutex number_datums_received_mu;
  int number_datums_received = 0;
  client->GetCloudWatchClient()->put_metric_data_async_mock =
      [&](const Aws::CloudWatch::Model::PutMetricDataRequest& request,
          const Aws::CloudWatch::PutMetricDataResponseReceivedHandler& handler,
          const std::shared_ptr<const Aws::Client::AsyncCallerContext>&
              context) {
        EXPECT_THAT(request.GetNamespace(), StrEq(kNamespace));
        put_metric_data_request_count.DecrementCount();
        absl::MutexLock l(&number_datums_received_mu);
        number_datums_received += request.GetMetricData().size();
      };

  auto requests_vector = std::make_shared<
      std::vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>();
  for (auto metric_num : {100, 500, 600, 800}) {
    PutMetricsRequest record_metric_request;
    SetPutMetricsRequest(record_metric_request, kValue, metric_num);

    AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
        std::make_shared<PutMetricsRequest>(record_metric_request),
        [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});
    requests_vector->push_back(context);
  }
  EXPECT_SUCCESS(client->MetricsBatchPush(requests_vector));
  put_metric_data_request_count.Wait();
  {
    absl::MutexLock l(&number_datums_received_mu);
    auto condition_fn = [&] {
      number_datums_received_mu.AssertReaderHeld();
      return number_datums_received == 2000;
    };
    number_datums_received_mu.Await(absl::Condition(&condition_fn));
  }

  // Cannot stop the client because the AWS callback is mocked.
}

TEST_F(AwsMetricClientProviderTest, OnPutMetricDataAsyncCallbackWithError) {
  auto client = CreateClient(true);

  client->GetInstanceClientProvider()->instance_resource_name =
      kResourceNameMock;
  EXPECT_SUCCESS(client->Init());
  EXPECT_SUCCESS(client->Run());

  AWSError<CloudWatchErrors> error(CloudWatchErrors::UNKNOWN, false);
  client->GetCloudWatchClient()->put_metric_data_outcome_mock =
      PutMetricDataOutcome(error);

  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request);
  absl::BlockingCounter context_finish_count(3);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      std::make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {
        context_finish_count.DecrementCount();
        EXPECT_THAT(
            context.result,
            ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));
      });
  auto requests_vector = std::make_shared<
      std::vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>();
  requests_vector->push_back(context);
  requests_vector->push_back(context);
  requests_vector->push_back(context);
  EXPECT_SUCCESS(client->MetricsBatchPush(requests_vector));
  context_finish_count.Wait();

  // Cannot stop the client because the AWS callback is mocked.
}

TEST_F(AwsMetricClientProviderTest, OnPutMetricDataAsyncCallbackWithSuccess) {
  auto client = CreateClient(true);

  client->GetInstanceClientProvider()->instance_resource_name =
      kResourceNameMock;
  EXPECT_SUCCESS(client->Init());
  EXPECT_SUCCESS(client->Run());

  Aws::NoResult result;
  client->GetCloudWatchClient()->put_metric_data_outcome_mock =
      PutMetricDataOutcome(result);

  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request);
  absl::BlockingCounter context_finish_count(3);
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      std::make_shared<PutMetricsRequest>(record_metric_request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {
        context_finish_count.DecrementCount();
        EXPECT_SUCCESS(context.result);
      });
  auto requests_vector = std::make_shared<
      std::vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>();
  requests_vector->push_back(context);
  requests_vector->push_back(context);
  requests_vector->push_back(context);
  EXPECT_SUCCESS(client->MetricsBatchPush(requests_vector));
  context_finish_count.Wait();

  // Cannot stop the client because the AWS callback is mocked.
}

TEST_F(AwsMetricClientProviderTest,
       MultipleMetricsWithoutBatchRecordingShouldFail) {
  auto client = CreateClient(false);

  client->GetInstanceClientProvider()->instance_resource_name =
      kResourceNameMock;

  EXPECT_SUCCESS(client->Init());
  EXPECT_SUCCESS(client->Run());

  auto requests_vector = std::make_shared<
      std::vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>();
  for (auto metric_num : {100, 500, 600, 800}) {
    PutMetricsRequest record_metric_request;
    SetPutMetricsRequest(record_metric_request, kValue, metric_num);

    AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
        std::make_shared<PutMetricsRequest>(record_metric_request),
        [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {});
    requests_vector->push_back(context);
  }
  EXPECT_THAT(
      client->MetricsBatchPush(requests_vector),
      ResultIs(FailureExecutionResult(
          SC_AWS_METRIC_CLIENT_PROVIDER_SHOULD_ENABLE_BATCH_RECORDING)));
  EXPECT_SUCCESS(client->Stop());
}

TEST_F(AwsMetricClientProviderTest, OneMetricWithoutBatchRecordingSucceed) {
  auto client = CreateClient(false);

  client->GetInstanceClientProvider()->instance_resource_name =
      kResourceNameMock;

  EXPECT_SUCCESS(client->Init());
  EXPECT_SUCCESS(client->Run());

  Aws::NoResult result;
  client->GetCloudWatchClient()->put_metric_data_outcome_mock =
      PutMetricDataOutcome(result);

  auto requests_vector = std::make_shared<
      std::vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>();
  PutMetricsRequest record_metric_request;
  SetPutMetricsRequest(record_metric_request, kValue, 100);

  requests_vector->push_back(
      AsyncContext<PutMetricsRequest, PutMetricsResponse>(
          std::make_shared<PutMetricsRequest>(record_metric_request),
          [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>& context) {
            EXPECT_SUCCESS(context.result);
          }));

  EXPECT_SUCCESS(client->MetricsBatchPush(requests_vector));
  EXPECT_SUCCESS(client->Stop());
}
}  // namespace google::scp::cpio::client_providers::test
