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

#include "aws_metric_client_provider.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <aws/monitoring/CloudWatchClient.h>
#include <aws/monitoring/CloudWatchErrors.h>
#include <aws/monitoring/model/PutMetricDataRequest.h>

#include "absl/base/nullability.h"
#include "absl/functional/bind_front.h"
#include "src/core/async_executor/aws/aws_async_executor.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/cpio/client_providers/instance_client_provider/aws/aws_instance_client_utils.h"
#include "src/cpio/client_providers/interface/metric_client_provider_interface.h"
#include "src/cpio/common/aws/aws_utils.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/metric_client/metric_client_interface.h"
#include "src/public/cpio/interface/metric_client/type_def.h"
#include "src/public/cpio/proto/metric_service/v1/metric_service.pb.h"

#include "aws_metric_client_utils.h"
#include "cloud_watch_error_converter.h"
#include "error_codes.h"

using Aws::Client::AsyncCallerContext;
using Aws::Client::ClientConfiguration;
using Aws::CloudWatch::CloudWatchClient;
using Aws::CloudWatch::CloudWatchErrors;
using Aws::CloudWatch::Model::MetricDatum;
using Aws::CloudWatch::Model::PutMetricDataOutcome;
using Aws::CloudWatch::Model::PutMetricDataRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::protobuf::Any;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::FinishContext;
using google::scp::core::MessageRouterInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_AWS_METRIC_CLIENT_PROVIDER_REQUEST_PAYLOAD_OVERSIZE;
using google::scp::core::errors::
    SC_AWS_METRIC_CLIENT_PROVIDER_SHOULD_ENABLE_BATCH_RECORDING;
using google::scp::cpio::client_providers::AwsInstanceClientUtils;
using google::scp::cpio::common::CreateClientConfiguration;

namespace {
// Specifies the maximum number of HTTP connections to a single server.
constexpr size_t kCloudwatchMaxConcurrentConnections = 50;
// The limit of AWS PutMetricDataRequest metric datum is 1000.
constexpr size_t kAwsMetricDatumSizeLimit = 1000;
// The Aws PutMetricDataRequest payload size limit is about 1MB.
constexpr size_t kAwsPayloadSizeLimit = 1024 * 1024;
constexpr std::string_view kAwsMetricClientProvider = "AwsMetricClientProvider";
}  // namespace

namespace google::scp::cpio::client_providers {
ClientConfiguration AwsMetricClientProvider::GetClientConfig(
    std::string_view region) noexcept {
  ClientConfiguration client_config;
  client_config = common::CreateClientConfiguration(region);
  client_config.executor =
      std::make_shared<AwsAsyncExecutor>(io_async_executor_);
  client_config.maxConnections = kCloudwatchMaxConcurrentConnections;
  return client_config;
}

absl::Status AwsMetricClientProvider::Init() noexcept {
  if (absl::Status error = MetricClientProvider::Init(); !error.ok()) {
    SCP_ERROR(kAwsMetricClientProvider, kZeroUuid, error,
              "Failed to initialize MetricClientProvider");
    return error;
  }
  if (region_.empty()) {
    auto region_code_or = AwsInstanceClientUtils::GetCurrentRegionCode(
        *instance_client_provider_);

    if (!region_code_or.Successful()) {
      SCP_ERROR(kAwsMetricClientProvider, kZeroUuid, region_code_or.result(),
                "Failed to get region code for current instance");
      return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
          region_code_or.result().status_code));
    }

    SCP_INFO(kAwsMetricClientProvider, kZeroUuid, "GetCurrentRegionCode: %s",
             region_code_or->c_str());
    cloud_watch_client_.emplace(GetClientConfig(*region_code_or));
  } else {
    cloud_watch_client_.emplace(GetClientConfig(region_));
  }
  return absl::OkStatus();
}

ExecutionResult AwsMetricClientProvider::MetricsBatchPush(
    const std::shared_ptr<
        std::vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>&
        metric_requests_vector) noexcept {
  if (metric_requests_vector->empty()) {
    return SuccessExecutionResult();
  }

  // When perform batch recording, enable_batch_recording should be true.
  if (!metric_batching_options_.enable_batch_recording &&
      metric_requests_vector->size() > 1) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_METRIC_CLIENT_PROVIDER_SHOULD_ENABLE_BATCH_RECORDING);
    SCP_ERROR(kAwsMetricClientProvider, kZeroUuid, execution_result,
              "Should enable batch recording");
    return execution_result;
  }

  std::vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>
      context_chunk;

  PutMetricDataRequest request_chunk;
  // Already confirmed if metric_namespace in metric_batching_options is empty,
  // batch recording is not enabled and metric_requests_vector should only have
  // one entry.
  std::string name_space =
      !metric_batching_options_.metric_namespace.empty()
          ? metric_batching_options_.metric_namespace
          : (*metric_requests_vector)[0].request->metric_namespace();
  request_chunk.SetNamespace(name_space.c_str());
  size_t chunk_payload = 0;
  size_t chunk_size = 0;

  auto context_size = metric_requests_vector->size();
  for (size_t i = 0; i < context_size; i++) {
    auto context = metric_requests_vector->at(i);
    std::vector<MetricDatum> datum_list;
    auto result = AwsMetricClientUtils::ParseRequestToDatum(
        context, datum_list, kAwsMetricDatumSizeLimit);

    // Skips the context that failed in ParseRequestToDatum().
    if (!result.Successful()) {
      SCP_ERROR_CONTEXT(kAwsMetricClientProvider, context, result,
                        "Invalid metric.");
      continue;
    }

    // Single request payload size cannot be greater than kAwsPayloadSizeLimit.
    PutMetricDataRequest datums_piece;
    datums_piece.SetNamespace(name_space.c_str());
    datums_piece.SetMetricData(datum_list);
    auto datums_payload = datums_piece.SerializePayload().length();
    if (datums_payload > kAwsPayloadSizeLimit) {
      auto execution_result = FailureExecutionResult(
          SC_AWS_METRIC_CLIENT_PROVIDER_REQUEST_PAYLOAD_OVERSIZE);
      SCP_ERROR_CONTEXT(kAwsMetricClientProvider, context, execution_result,
                        "Invalid metric.");
      context.Finish(execution_result);
      continue;
    }

    // Pushes the request chunk before chunk's size or payload exceeds the
    // thresholds.
    auto datums_size = datum_list.size();
    if (chunk_size + datums_size > kAwsMetricDatumSizeLimit ||
        chunk_payload + datums_payload > kAwsPayloadSizeLimit) {
      cloud_watch_client_->PutMetricDataAsync(
          request_chunk,
          absl::bind_front(
              &AwsMetricClientProvider::OnPutMetricDataAsyncCallback, this,
              context_chunk));
      {
        absl::MutexLock lock(&sync_mutex_);
        active_push_count_++;
      }

      // Resets all chunks.
      chunk_size = 0;
      chunk_payload = 0;
      request_chunk.SetMetricData({});
      context_chunk.clear();
    }

    chunk_size += datums_size;
    chunk_payload += datums_payload;
    for (auto& datum : datum_list) {
      request_chunk.AddMetricData(datum);
    }
    context_chunk.push_back(context);
  }

  // Pushes the remaining metrics in the chunk.
  if (!context_chunk.empty()) {
    cloud_watch_client_->PutMetricDataAsync(
        request_chunk,
        absl::bind_front(&AwsMetricClientProvider::OnPutMetricDataAsyncCallback,
                         this, context_chunk));
    absl::MutexLock lock(&sync_mutex_);
    active_push_count_++;
  }

  return SuccessExecutionResult();
}

void AwsMetricClientProvider::OnPutMetricDataAsyncCallback(
    std::vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>
        metric_requests_vector,
    const CloudWatchClient*, const PutMetricDataRequest&,
    const PutMetricDataOutcome& outcome,
    const std::shared_ptr<const AsyncCallerContext>&) noexcept {
  {
    absl::MutexLock lock(&sync_mutex_);
    active_push_count_--;
  }
  if (outcome.IsSuccess()) {
    for (auto& record_metric_context : metric_requests_vector) {
      record_metric_context.response = std::make_shared<PutMetricsResponse>();
      FinishContext(SuccessExecutionResult(), record_metric_context,
                    *async_executor_);
    }
    return;
  }

  // TODO(b/240477800): map HttpErrorCodes to local errors. For cloudwatch,
  // watch out HttpResponseCode::REQUEST_ENTITY_TOO_LARGE.
  auto result = CloudWatchErrorConverter::ConvertCloudWatchError(
      outcome.GetError().GetErrorType(), outcome.GetError().GetMessage());
  SCP_ERROR_CONTEXT(kAwsMetricClientProvider, metric_requests_vector.back(),
                    result, "The error is %s",
                    outcome.GetError().GetMessage().c_str());
  for (auto& record_metric_context : metric_requests_vector) {
    FinishContext(result, record_metric_context, *async_executor_);
  }
  return;
}

std::unique_ptr<MetricClientProviderInterface>
MetricClientProviderFactory::Create(
    MetricClientOptions options,
    absl::Nonnull<InstanceClientProviderInterface*> instance_client_provider,
    absl::Nonnull<AsyncExecutorInterface*> async_executor,
    absl::Nonnull<AsyncExecutorInterface*> io_async_executor) {
  return std::make_unique<AwsMetricClientProvider>(
      std::move(options), instance_client_provider, async_executor,
      io_async_executor);
}
}  // namespace google::scp::cpio::client_providers
