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

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/synchronization/notification.h"
#include "src/public/core/interface/errors.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/cpio.h"
#include "src/public/cpio/interface/metric_client/metric_client_interface.h"
#include "src/public/cpio/interface/metric_client/type_def.h"
#include "src/public/cpio/interface/type_def.h"
#include "src/public/cpio/proto/metric_service/v1/metric_service.pb.h"

using google::cmrt::sdk::metric_service::v1::MetricUnit;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::LogOption;
using google::scp::cpio::MetricClientFactory;
using google::scp::cpio::MetricClientInterface;
using google::scp::cpio::MetricClientOptions;

int main(int argc, char* argv[]) {
  CpioOptions cpio_options;
  cpio_options.log_option = LogOption::kConsoleLog;
  auto result = Cpio::InitCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to initialize CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }

  MetricClientOptions metric_client_options;
  auto metric_client =
      MetricClientFactory::Create(std::move(metric_client_options));
  if (absl::Status error = metric_client->Init(); !error.ok()) {
    std::cout << "Cannot init metric client!" << error << std::endl;
    return 0;
  }
  auto request = std::make_shared<PutMetricsRequest>();
  request->set_metric_namespace("test");
  auto metric = request->add_metrics();
  metric->set_name("test_metric");
  metric->set_value("12");
  metric->set_unit(MetricUnit::METRIC_UNIT_COUNT);
  auto& labels = *metric->mutable_labels();
  labels["label_key"] = "label_value";

  absl::Notification finished;
  auto context = AsyncContext<PutMetricsRequest, PutMetricsResponse>(
      std::move(request),
      [&](AsyncContext<PutMetricsRequest, PutMetricsResponse> context) {
        if (!context.result.Successful()) {
          std::cout << "PutMetrics failed: "
                    << GetErrorMessage(context.result.status_code) << std::endl;
        } else {
          std::cout << "PutMetrics succeeded." << std::endl;
        }
        finished.Notify();
      });
  if (absl::Status error = metric_client->PutMetrics(context); !error.ok()) {
    std::cout << "PutMetrics failed immediately: " << error << std::endl;
  }
  finished.WaitForNotificationWithTimeout(absl::Seconds(100));
  result = Cpio::ShutdownCpio(cpio_options);
  if (!result.Successful()) {
    std::cout << "Failed to shutdown CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
  }
}
