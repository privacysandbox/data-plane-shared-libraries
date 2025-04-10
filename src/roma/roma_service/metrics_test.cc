/*
 * Copyright 2023 Google LLC
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

#include "src/roma/interface/metrics.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"
#include "src/util/duration.h"

using google::scp::roma::kExecutionMetricActiveWorkerRatio;
using google::scp::roma::kExecutionMetricDurationMs;
using google::scp::roma::kExecutionMetricJsEngineCallDurationMs;
using google::scp::roma::kExecutionMetricQueueFullnessRatio;
using google::scp::roma::kExecutionMetricWaitTimeMs;
using google::scp::roma::kHandlerCallMetricJsEngineDurationMs;
using google::scp::roma::kInputParsingMetricJsEngineDurationMs;
using google::scp::roma::sandbox::roma_service::RomaService;
using ::testing::_;
using ::testing::DoubleNear;
using ::testing::Gt;
using ::testing::StrEq;

namespace google::scp::roma::test {
namespace {

TEST(MetricsTest, ShouldGetMetricsInResponse) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE",
    });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response_status = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(response_status.ok());
  }

  // Add 5 second of delay to ensure that all workers have finished loading the
  // UDF before executing the handler.
  absl::SleepFor(absl::Seconds(5));

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("Foobar")"},
        });

    absl::Status response_status;
    absl::flat_hash_map<std::string, double> metrics;
    ASSERT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response_status = resp.status();
                               if (resp.ok()) {
                                 result = std::move(resp->resp);
                               }

                               metrics = resp->metrics;

                               std::cout << "Metrics:" << std::endl;
                               for (const auto& pair : resp->metrics) {
                                 std::cout << pair.first << ": " << pair.second
                                           << std::endl;
                               }

                               execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(response_status.ok());

    // Use 10ms as a generous upper bound for all metrics.
    int duration_upper_bound = 10;
    EXPECT_GT(metrics[kExecutionMetricWaitTimeMs], 0);
    EXPECT_LT(metrics[kExecutionMetricWaitTimeMs], duration_upper_bound);
    EXPECT_GT(metrics[kExecutionMetricDurationMs], 0);
    EXPECT_LT(metrics[kExecutionMetricDurationMs], duration_upper_bound);
    EXPECT_EQ(metrics[kExecutionMetricQueueFullnessRatio], 0);
    EXPECT_THAT(metrics[kExecutionMetricActiveWorkerRatio],
                DoubleNear(0.5, 0.0001));
    EXPECT_GT(metrics[kExecutionMetricJsEngineCallDurationMs], 0);
    EXPECT_LT(metrics[kExecutionMetricJsEngineCallDurationMs],
              duration_upper_bound);
    EXPECT_GT(metrics[kInputParsingMetricJsEngineDurationMs], 0);
    EXPECT_LT(metrics[kInputParsingMetricJsEngineDurationMs],
              duration_upper_bound);
    EXPECT_GT(metrics[kHandlerCallMetricJsEngineDurationMs], 0);
    EXPECT_LT(metrics[kHandlerCallMetricJsEngineDurationMs],
              duration_upper_bound);
  }

  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(MetricsTest, ActiveWorkerRatioMetricIsValid) {
  Config config;
  int num_workers = 4;
  config.number_of_workers = num_workers;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());
  absl::Notification load_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) {
      const sleepDurationMs = parseInt(input);
      const startTime = Date.now();
      while (Date.now() - startTime < sleepDurationMs) {}
    }
  )JS_CODE",
    });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response_status = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(response_status.ok());
  }

  std::vector<absl::Status> response_statuses(num_workers);
  std::vector<absl::Notification> notifications(num_workers);
  std::vector<double> active_worker_ratios(num_workers);
  for (int i = 0; i < num_workers; i++) {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {absl::StrCat("\"", (i + 1) * 1000, "\"")},
        });

    ASSERT_TRUE(
        roma_service
            .Execute(std::move(execution_obj),
                     [&, i](absl::StatusOr<ResponseObject> resp) {
                       response_statuses[i] = resp.status();
                       active_worker_ratios[i] =
                           resp->metrics[kExecutionMetricActiveWorkerRatio];

                       notifications[i].Notify();
                     })
            .ok());
  }

  for (int i = 0; i < num_workers; i++) {
    ASSERT_TRUE(
        notifications[i].WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(response_statuses[i].ok());
  }

  // Request i sleeps for (i+1)*1000 seconds, so when request i finishes, all
  // other requests are still running, therefore expected active worker ratio
  // would be 1 - i/num_workers.
  for (int i = 0; i < num_workers; i++) {
    EXPECT_THAT(active_worker_ratios[i],
                DoubleNear(1 - (static_cast<double>(i) / num_workers), 0.0001));
  }

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(MetricsTest, QueueFullnessRatioMetricIsValid) {
  Config config;
  config.number_of_workers = 1;
  int max_queue_size = 10;
  config.worker_queue_max_items = max_queue_size;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());
  absl::Notification load_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) {
      const sleepDurationMs = parseInt(input);
      const startTime = Date.now();
      while (Date.now() - startTime < sleepDurationMs) {}
    }
  )JS_CODE",
    });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response_status = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(response_status.ok());
  }

  std::vector<absl::Status> response_statuses(max_queue_size);
  std::vector<absl::Notification> notifications(max_queue_size);
  std::vector<double> queue_fullness_ratios(max_queue_size);
  for (int i = 0; i < max_queue_size; i++) {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("1000")"},
        });

    ASSERT_TRUE(
        roma_service
            .Execute(std::move(execution_obj),
                     [&, i](absl::StatusOr<ResponseObject> resp) {
                       response_statuses[i] = resp.status();
                       queue_fullness_ratios[i] =
                           resp->metrics[kExecutionMetricQueueFullnessRatio];

                       notifications[i].Notify();
                     })
            .ok());
  }

  for (int i = 0; i < max_queue_size; i++) {
    ASSERT_TRUE(
        notifications[i].WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(response_statuses[i].ok());
  }

  for (int i = 0; i < max_queue_size; i++) {
    // Queue fullness ratio should be 0.9 for the first request, 0.8 for the
    // second, etc.
    EXPECT_THAT(queue_fullness_ratios[i], DoubleNear(0.9 - i * 0.1, 0.0001));
  }

  ASSERT_TRUE(roma_service.Stop().ok());
}

TEST(MetricsTest, QueueingDurationReturnedAsMetric) {
  Config config;
  config.number_of_workers = 1;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  absl::Notification load_finished;
  absl::Notification execute_finished1;
  absl::Notification execute_finished2;
  absl::Status response_status1;
  absl::Status response_status2;
  std::string result;
  size_t sleep_duration_ms = 50;
  absl::Duration queueing_duration;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(duration_ms) {
      const start = performance.now();
      // Hang to ensure worker is fully occupied for duration_ms.
      while (performance.now() - start < duration_ms) {}
      return "Hello world";
    }
  )JS_CODE",
    });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response_status = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(response_status.ok());
  }

  {
    InvocationStrRequest<> execution_obj{
        .id = "foo",
        .version_string = "v1",
        .handler_name = "Handler",
        .input = {absl::StrCat("\"", sleep_duration_ms, "\"")},
    };

    ASSERT_TRUE(
        roma_service
            .Execute(std::make_unique<InvocationStrRequest<>>(execution_obj),
                     [&](absl::StatusOr<ResponseObject> resp) {
                       response_status1 = resp.status();
                       execute_finished1.Notify();
                     })
            .ok());

    ASSERT_TRUE(
        roma_service
            .Execute(std::make_unique<InvocationStrRequest<>>(execution_obj),
                     [&](absl::StatusOr<ResponseObject> resp) {
                       response_status2 = resp.status();
                       if (resp.ok()) {
                         result = std::move(resp->resp);
                       }

                       auto it = resp->metrics.find(kExecutionMetricWaitTimeMs);
                       std::cout << it->first << ":" << it->second << std::endl;
                       ASSERT_TRUE(it != resp->metrics.end());
                       queueing_duration = absl::Milliseconds(it->second);

                       execute_finished2.Notify();
                     })
            .ok());
  }
  ASSERT_TRUE(
      execute_finished1.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished2.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(response_status1.ok());
  ASSERT_TRUE(response_status2.ok());

  // With only one worker, the first execution should occupy the worker for
  // sleep_duration_ms, meaning the second request will sit in the queue until
  // the first execution finishes, so the queueing duration should be about
  // sleep_duration_ms.
  EXPECT_GT(queueing_duration, absl::Milliseconds(sleep_duration_ms));
  EXPECT_THAT(result, StrEq(R"("Hello world")"));

  ASSERT_TRUE(roma_service.Stop().ok());
}
}  // namespace
}  // namespace google::scp::roma::test
