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

#include "metric_client_provider.h"

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/functional/bind_front.h"
#include "google/protobuf/any.pb.h"
#include "src/core/common/time_provider/time_provider.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/cpio/client_providers/interface/type_def.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/metric_service/v1/metric_service.pb.h"

#include "error_codes.h"
#include "metric_client_utils.h"

using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::protobuf::Any;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::TimeProvider;
using google::scp::core::errors::
    SC_METRIC_CLIENT_PROVIDER_EXECUTOR_NOT_AVAILABLE;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_IS_ALREADY_RUNNING;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_IS_NOT_RUNNING;
using google::scp::core::errors::SC_METRIC_CLIENT_PROVIDER_NAMESPACE_NOT_SET;

namespace {
constexpr std::string_view kMetricClientProvider = "MetricClientProvider";
constexpr size_t kShutdownWaitIntervalMilliseconds = 100;
// The metrics size to trigger a batch push.
constexpr size_t kMetricsBatchSize = 1000;
}  // namespace

namespace google::scp::cpio::client_providers {

ExecutionResult MetricClientProvider::Init() noexcept {
  // Metric namespace cannot be empty when enable batch recording.
  if (is_batch_recording_enable &&
      metric_batching_options_.metric_namespace.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_METRIC_CLIENT_PROVIDER_NAMESPACE_NOT_SET);
    SCP_ERROR(kMetricClientProvider, kZeroUuid, execution_result,
              "Invalid namespace.");
    return execution_result;
  }

  if (is_batch_recording_enable && !async_executor_) {
    return FailureExecutionResult(
        SC_METRIC_CLIENT_PROVIDER_EXECUTOR_NOT_AVAILABLE);
  }

  return SuccessExecutionResult();
}

ExecutionResult MetricClientProvider::Run() noexcept {
  if (absl::MutexLock l(&sync_mutex_); is_running_) {
    auto execution_result =
        FailureExecutionResult(SC_METRIC_CLIENT_PROVIDER_IS_ALREADY_RUNNING);
    SCP_ERROR(kMetricClientProvider, kZeroUuid, execution_result,
              "Failed to run MetricClientProvider.");
    return execution_result;
  } else {
    is_running_ = true;
  }

  if (is_batch_recording_enable) {
    return ScheduleMetricsBatchPush();
  }
  return SuccessExecutionResult();
}

ExecutionResult MetricClientProvider::Stop() noexcept {
  {
    absl::MutexLock l(&sync_mutex_);
    is_running_ = false;
    if (is_batch_recording_enable) {
      current_cancellation_callback_();
      // To push the remaining metrics in the vector.
      RunMetricsBatchPush();
    }
  }

  while (active_push_count_ > 0) {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(kShutdownWaitIntervalMilliseconds));
  }

  return SuccessExecutionResult();
}

ExecutionResult MetricClientProvider::PutMetrics(
    AsyncContext<PutMetricsRequest, PutMetricsResponse>
        record_metric_context) noexcept {
  absl::MutexLock l(&sync_mutex_);
  if (!is_running_) {
    auto execution_result =
        FailureExecutionResult(SC_METRIC_CLIENT_PROVIDER_IS_NOT_RUNNING);
    SCP_ERROR_CONTEXT(kMetricClientProvider, record_metric_context,
                      execution_result, "Failed to record metric.");
    record_metric_context.Finish(execution_result);
    return execution_result;
  }

  auto execution_result = MetricClientUtils::ValidateRequest(
      *record_metric_context.request, metric_batching_options_);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kMetricClientProvider, record_metric_context,
                      execution_result, "Invalid metric.");
    record_metric_context.Finish(execution_result);
    return execution_result;
  }

  // In following actions:
  //    1. push back record_metric_context into metric_requests_vector_.
  //    2. if the condition satisfied, execute RunMetricsBatchPush()
  //    RunMetricsBatchPush() swaps metric_requests_vector_ for a vector being
  //    pushed to the cloud.
  // The above two actions should be atomic, so the mutex is protecting them.
  metric_requests_vector_.push_back(record_metric_context);
  number_metrics_in_vector_ += record_metric_context.request->metrics().size();

  /**
   * @brief Metrics pushed when batch disable or the number of metrics is over
   * kMetricsBatchSize. When batch enabled, kMetricsBatchSize is used to avoid
   * excessive memory usage by storing too many metrics in the vector when the
   * batch schedule time duration is too large.
   */
  if (!is_batch_recording_enable ||
      number_metrics_in_vector_ >= kMetricsBatchSize) {
    RunMetricsBatchPush();
  }
  return SuccessExecutionResult();
}

void MetricClientProvider::RunMetricsBatchPush() noexcept {
  auto requests_vector_copy = std::make_shared<
      std::vector<AsyncContext<PutMetricsRequest, PutMetricsResponse>>>();
  metric_requests_vector_.swap(*requests_vector_copy);
  number_metrics_in_vector_ = 0;

  if (requests_vector_copy->empty()) {
    return;
  }
  auto execution_result = MetricsBatchPush(requests_vector_copy);
  if (!execution_result.Successful()) {
    SCP_ERROR(kMetricClientProvider, kZeroUuid, execution_result,
              "Failed to push metrics in batch.");
  }
  return;
}

ExecutionResult MetricClientProvider::ScheduleMetricsBatchPush() noexcept {
  if (absl::MutexLock l(&sync_mutex_); !is_running_) {
    auto execution_result =
        FailureExecutionResult(SC_METRIC_CLIENT_PROVIDER_IS_NOT_RUNNING);
    SCP_ERROR(kMetricClientProvider, kZeroUuid, execution_result,
              "Failed to schedule metric batch push.");
    return execution_result;
  }

  auto next_push_time = (TimeProvider::GetSteadyTimestampInNanoseconds() +
                         metric_batching_options_.batch_recording_time_duration)
                            .count();
  auto execution_result = async_executor_->ScheduleFor(
      [this]() {
        ScheduleMetricsBatchPush();

        absl::MutexLock l(&sync_mutex_);
        RunMetricsBatchPush();
      },
      next_push_time, current_cancellation_callback_);
  if (!execution_result.Successful()) {
    SCP_ERROR(kMetricClientProvider, kZeroUuid, execution_result,
              "Failed to schedule metric batch push.");
  }
  return execution_result;
}

void MetricClientProvider::OnPutMetrics(
    AsyncContext<Any, Any> any_context) noexcept {
  auto request = std::make_shared<PutMetricsRequest>();
  any_context.request->UnpackTo(request.get());
  AsyncContext<PutMetricsRequest, PutMetricsResponse> context(
      std::move(request),
      absl::bind_front(
          CallbackToPackAnyResponse<PutMetricsRequest, PutMetricsResponse>,
          any_context),
      any_context);
  context.result = PutMetrics(context);
}
}  // namespace google::scp::cpio::client_providers
