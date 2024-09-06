
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

#ifndef CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_METRIC_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_METRIC_CLIENT_PROVIDER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "google/protobuf/any.pb.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "src/cpio/client_providers/interface/metric_client_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/metric_client/metric_client_interface.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc MetricClientInterface
 */
class MetricClientProvider : public MetricClientProviderInterface {
 public:
  explicit MetricClientProvider(
      absl::Nonnull<core::AsyncExecutorInterface*> async_executor,
      MetricBatchingOptions metric_batching_options = MetricBatchingOptions())
      : async_executor_(async_executor),
        metric_batching_options_(std::move(metric_batching_options)),
        is_batch_recording_enable(
            metric_batching_options_.enable_batch_recording),
        active_push_count_(0),
        number_metrics_in_vector_(0) {}

  ~MetricClientProvider() override;

  absl::Status Init() noexcept override;

  absl::Status PutMetrics(
      core::AsyncContext<cmrt::sdk::metric_service::v1::PutMetricsRequest,
                         cmrt::sdk::metric_service::v1::PutMetricsResponse>
          record_metric_context) noexcept override
      ABSL_LOCKS_EXCLUDED(sync_mutex_);

 protected:
  /**
   * @brief Triggered when PutMetricsRequest arrives.
   *
   * @param context async execution context.
   */
  virtual void OnPutMetrics(
      core::AsyncContext<google::protobuf::Any, google::protobuf::Any>
          context) noexcept;

  /**
   * @brief The actual function to push the metrics received to cloud.
   *
   * @param metric_requests_vector The vector stored record_metric_request
   * contexts.
   */
  virtual core::ExecutionResult MetricsBatchPush(
      const std::shared_ptr<std::vector<core::AsyncContext<
          cmrt::sdk::metric_service::v1::PutMetricsRequest,
          cmrt::sdk::metric_service::v1::PutMetricsResponse>>>&
          metric_requests_vector) noexcept = 0;

  /**
   * @brief Schedules a round of metric push in the next time_duration_. The
   * main goal of this feature is to ensure that when the client does not
   * receive enough metrics to trigger a batch push, the metrics will be pushed
   * to the cloud in set time duration.
   *
   * @return core::ExecutionResult
   */
  virtual core::ExecutionResult ScheduleMetricsBatchPush() noexcept;

  /**
   * @brief The helper function for ScheduleMetricsBatchPush to do the actual
   * metrics batching and pushing.
   *
   */
  virtual void RunMetricsBatchPush() noexcept
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(sync_mutex_);

  /// An instance to the async executor.
  core::AsyncExecutorInterface* async_executor_;
  MetricBatchingOptions metric_batching_options_;

  /// Whether metric client enables batch recording.
  bool is_batch_recording_enable;

  /// The vector stores the metric record requests received. Any changes to this
  /// vector should be thread-safe.
  std::vector<
      core::AsyncContext<cmrt::sdk::metric_service::v1::PutMetricsRequest,
                         cmrt::sdk::metric_service::v1::PutMetricsResponse>>
      metric_requests_vector_ ABSL_GUARDED_BY(sync_mutex_);

  /// Number of active metric push.
  size_t active_push_count_ ABSL_GUARDED_BY(sync_mutex_);
  /// Number of metrics received in metric_requests_vector_.
  uint64_t number_metrics_in_vector_ ABSL_GUARDED_BY(sync_mutex_);

  /// The cancellation callback.
  std::function<bool()> current_cancellation_callback_;

  /// Sync mutex.
  absl::Mutex sync_mutex_;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_METRIC_CLIENT_PROVIDER_H_
