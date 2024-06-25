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

#include "roma_benchmark.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <benchmark/benchmark.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/util/duration.h"
#include "src/util/status_macro/status_macros.h"

using google::scp::roma::CodeObject;
using google::scp::roma::Config;
using google::scp::roma::InvocationSharedRequest;
using google::scp::roma::ResponseObject;
using google::scp::roma::benchmark::BenchmarkMetrics;
using google::scp::roma::benchmark::InputsType;
using google::scp::roma::sandbox::constants::
    kExecutionMetricJsEngineCallDuration;
using google::scp::roma::sandbox::constants::
    kExecutionMetricSandboxedJsEngineCallDuration;
using google::scp::roma::sandbox::constants::kHandlerCallMetricJsEngineDuration;
using google::scp::roma::sandbox::constants::
    kInputParsingMetricJsEngineDuration;
using google::scp::roma::sandbox::roma_service::RomaService;

namespace {

const std::list<float> kPercentiles = {50, 90, 99, 99.99};

CodeObject CreateCodeObj(std::string_view code_string) {
  CodeObject code_obj;
  code_obj.id = "foo";
  code_obj.version_string = "v1";
  if (!code_string.empty()) {
    code_obj.js = code_string;
  } else {
    code_obj.js = "function Handler() { return \"Hello world!\";}";
  }

  return code_obj;
}

std::string FormatWithCommas(int value) {
  std::stringstream ss;
  ss.imbue(std::locale(""));
  ss << std::fixed << value;
  return ss.str();
}

std::string GenerateRandomString() {
  auto length = std::rand() % 9 + 1;
  std::string output;
  for (auto i = 0; i < length; i++) {
    output += 'a' + std::rand() % 26;
  }
  return output;
}

std::string GenerateRandomJsonString(size_t depth, size_t wide) {
  std::string output = "{";

  for (auto i = 0; i < wide; i++) {
    auto json_key = GenerateRandomString();
    output += "\"" + json_key + "\":";
    std::string value;
    if (depth == 1) {
      value = "\"" + GenerateRandomString() + "\"";
    } else {
      value = GenerateRandomJsonString(depth - 1, wide);
    }
    output += value;
    if (i != wide - 1) {
      output += ",";
    }
  }

  output += "}";
  return output;
}

InvocationSharedRequest<> CreateExecutionObj(InputsType type,
                                             size_t payload_size,
                                             size_t json_depth) {
  InvocationSharedRequest<> code_obj;
  code_obj.id = "foo";
  code_obj.version_string = "v1";
  code_obj.handler_name = "Handler";

  if (type == InputsType::kNestedJsonString) {
    std::string inputs_string =
        GenerateRandomJsonString(json_depth, 1 /*elements in each layer*/);
    code_obj.input.push_back(std::make_shared<std::string>(inputs_string));
    std::cout << "\tinputs size in Byte: " << inputs_string.length()
              << "\n\tinputs JSON depth: " << json_depth << std::endl;
  } else {
    std::string inputs_string(payload_size, 'A');
    code_obj.input.push_back(
        std::make_shared<std::string>("\"" + inputs_string + "\""));
    std::cout << "\tinputs size in Byte: " << inputs_string.length()
              << std::endl;
  }

  return code_obj;
}

void GetMetricFromResponse(const ResponseObject& resp,
                           BenchmarkMetrics& metrics) {
  if (const auto& it =
          resp.metrics.find(kExecutionMetricSandboxedJsEngineCallDuration);
      it != resp.metrics.end()) {
    metrics.sandbox_elapsed = it->second;
  }

  if (const auto& it = resp.metrics.find(kExecutionMetricJsEngineCallDuration);
      it != resp.metrics.end()) {
    metrics.v8_elapsed = it->second;
  }

  if (const auto& it = resp.metrics.find(kInputParsingMetricJsEngineDuration);
      it != resp.metrics.end()) {
    metrics.input_parsing_elapsed = it->second;
  }

  if (const auto& it = resp.metrics.find(kHandlerCallMetricJsEngineDuration);
      it != resp.metrics.end()) {
    metrics.handler_calling_elapse = it->second;
  }
}
}  // namespace

namespace google::scp::roma::benchmark {

void RomaBenchmarkSuite(const TestConfiguration& test_configuration) {
  Config config;
  config.number_of_workers = test_configuration.workers;
  config.worker_queue_max_items = test_configuration.queue_size;
  config.sandbox_request_response_shared_buffer_size_mb = 16;
  auto roma_service = std::make_unique<RomaService<>>(std::move(config));
  if (auto status = roma_service->Init(); !status.ok()) {
    std::cout << "Initializing Roma failed due to " << status.message()
              << std::endl;
    return;
  }

  std::cout << "\nRoma RunTest config:" << "\n\tworkers: "
            << test_configuration.workers
            << "\n\tqueue_size: " << test_configuration.queue_size
            << "\n\trequest_threads: " << test_configuration.request_threads
            << "\n\trequests per thread: "
            << test_configuration.requests_per_thread
            << "\n\tBatch size: " << test_configuration.batch_size << std::endl;

  if (auto status =
          LoadCodeObject(*roma_service, test_configuration.js_source_code);
      !status.ok()) {
    std::cout << "LoadCodeObject failed due to " << status.message()
              << std::endl;
    return;
  }

  auto test_execute_request = CreateExecutionObj(
      test_configuration.inputs_type, test_configuration.input_payload_in_byte,
      test_configuration.input_json_nested_depth);

  RomaBenchmark roma_benchmark(std::move(roma_service), test_execute_request,
                               test_configuration.batch_size,
                               test_configuration.request_threads,
                               test_configuration.requests_per_thread);

  roma_benchmark.RunTest();

  roma_benchmark.ConsoleTestMetrics();
}

BenchmarkMetrics BenchmarkMetrics::GetMeanMetrics(
    const std::vector<BenchmarkMetrics>& metrics) {
  auto num_metrics = metrics.size();
  BenchmarkMetrics mean_metric;
  for (size_t i = 0; i < num_metrics; i++) {
    mean_metric.total_execute_time += metrics[i].total_execute_time;
    mean_metric.sandbox_elapsed += metrics[i].sandbox_elapsed;
    mean_metric.v8_elapsed += metrics[i].v8_elapsed;
    mean_metric.input_parsing_elapsed += metrics[i].input_parsing_elapsed;
    mean_metric.handler_calling_elapse += metrics[i].handler_calling_elapse;
  }

  mean_metric.total_execute_time /= num_metrics;
  mean_metric.sandbox_elapsed /= num_metrics;
  mean_metric.v8_elapsed /= num_metrics;
  mean_metric.input_parsing_elapsed /= num_metrics;
  mean_metric.handler_calling_elapse /= num_metrics;

  return mean_metric;
}

absl::Status LoadCodeObject(RomaService<>& roma_service,
                            std::string_view code_string) {
  // Loads code object to Roma workers.
  auto code_obj = CreateCodeObj(code_string);
  std::promise<void> done;
  std::atomic_bool load_success{false};
  PS_RETURN_IF_ERROR(roma_service.LoadCodeObj(
      std::make_unique<CodeObject>(code_obj),
      [&](absl::StatusOr<ResponseObject> resp) {
        if (resp.ok()) {
          load_success = true;
        } else {
          std::cout << "LoadCodeObj failed with " << resp.status().message()
                    << std::endl;
        }
        done.set_value();
      }));

  done.get_future().get();
  if (load_success) {
    return absl::OkStatus();
  } else {
    return absl::InternalError("Roma failed to load code object ");
  }
}

RomaBenchmark::RomaBenchmark(
    std::unique_ptr<google::scp::roma::sandbox::roma_service::RomaService<>>
        roma_service,
    const InvocationSharedRequest<>& test_request, size_t batch_size,
    size_t threads, size_t requests_per_thread)
    : code_obj_(test_request),
      threads_(threads),
      batch_size_(batch_size),
      requests_per_thread_(requests_per_thread),
      latency_metrics_(threads * requests_per_thread, BenchmarkMetrics()),
      roma_service_(std::move(roma_service)) {}

RomaBenchmark::~RomaBenchmark() {
  if (auto status = roma_service_->Stop(); !status.ok()) {
    std::cout << "Stopping Roma failed due to " << status.message()
              << std::endl;
  }
}

void RomaBenchmark::RunTest() {
  privacy_sandbox::server_common::Stopwatch stopwatch;

  // Number of threads to send execute request.
  auto work_threads = std::vector<std::thread>();
  work_threads.reserve(threads_);
  for (auto i = 0; i < threads_; i++) {
    if (batch_size_ > 1) {
      work_threads.push_back(
          std::thread(&RomaBenchmark::SendRequestBatch, this));
    } else {
      work_threads.push_back(std::thread(&RomaBenchmark::SendRequest, this));
    }
  }

  for (auto& t : work_threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  // wait until all requests got response.
  while (success_requests_ + failed_requests_ <
         threads_ * requests_per_thread_) {
  }
  elapsed_time_ = stopwatch.GetElapsedTime();
}

void RomaBenchmark::ConsoleTestMetrics() {
  auto empty_spots = threads_ * requests_per_thread_ - success_requests_;
  for (auto i = 0; i < empty_spots; i++) {
    latency_metrics_.pop_back();
  }
  std::cout << "\n Elapsed time: " << absl::ToInt64Nanoseconds(elapsed_time_)
            << " ns" << std::endl;
  std::cout << "\nNative Roma e2e total finished Requests: "
            << FormatWithCommas(success_requests_ + failed_requests_)
            << std::endl;
  std::cout << "Success Requests: " << FormatWithCommas(success_requests_)
            << std::endl;
  std::cout << "Failed Requests: " << FormatWithCommas(failed_requests_)
            << std::endl;

  std::cout << "RPS: "
            << FormatWithCommas((success_requests_ + failed_requests_) /
                                absl::ToInt64Seconds(elapsed_time_))
            << std::endl;

  auto average_metric = BenchmarkMetrics::GetMeanMetrics(latency_metrics_);
  std::cout << "\nMean metrics:" << std::endl;
  std::cout << "\te2e execution time: "
            << absl::ToInt64Nanoseconds(average_metric.total_execute_time)
            << " ns" << std::endl;
  std::cout << "\tSandbox elapsed: "
            << absl::ToInt64Nanoseconds(average_metric.sandbox_elapsed) << " ns"
            << std::endl;
  std::cout << "\tV8 elapsed: "
            << absl::ToInt64Nanoseconds(average_metric.v8_elapsed) << " ns"
            << std::endl;
  std::cout << "\tInput parsing elapsed: "
            << absl::ToInt64Nanoseconds(average_metric.input_parsing_elapsed)
            << " ns" << std::endl;
  std::cout << "\tHandler function calling elapsed: "
            << absl::ToInt64Nanoseconds(average_metric.handler_calling_elapse)
            << " ns\n"
            << std::endl;

  {
    std::sort(latency_metrics_.begin(), latency_metrics_.end(),
              BenchmarkMetrics::CompareByTotalExec);
    std::cout << "e2e execution Elapsed: " << std::endl;
    for (auto& p : kPercentiles) {
      auto index = latency_metrics_.size() / 100 * p;

      std::cout << "\t" << p << "th percentile: "
                << absl::ToInt64Nanoseconds(
                       latency_metrics_.at(index).total_execute_time)
                << " ns" << std::endl;
    }
  }

  {
    std::sort(latency_metrics_.begin(), latency_metrics_.end(),
              BenchmarkMetrics::CompareBySandboxElapsed);
    std::cout << "Sandbox Elapsed: " << std::endl;
    for (auto& p : kPercentiles) {
      auto index = latency_metrics_.size() / 100 * p;

      std::cout << "\t" << p << "th percentile: "
                << absl::ToInt64Nanoseconds(
                       latency_metrics_.at(index).sandbox_elapsed)
                << " ns" << std::endl;
    }
  }

  {
    std::sort(latency_metrics_.begin(), latency_metrics_.end(),
              BenchmarkMetrics::CompareByV8Elapsed);
    std::cout << "V8 Elapsed: " << std::endl;
    for (auto& p : kPercentiles) {
      auto index = latency_metrics_.size() / 100 * p;

      std::cout << "\t" << p << "th percentile: "
                << absl::ToInt64Nanoseconds(
                       latency_metrics_.at(index).v8_elapsed)
                << " ns" << std::endl;
    }
  }

  {
    std::sort(latency_metrics_.begin(), latency_metrics_.end(),
              BenchmarkMetrics::CompareByInputsParsingElapsed);
    std::cout << "Inputs parsing Elapsed: " << std::endl;
    for (auto& p : kPercentiles) {
      auto index = latency_metrics_.size() / 100 * p;

      std::cout << "\t" << p << "th percentile: "
                << absl::ToInt64Nanoseconds(
                       latency_metrics_.at(index).input_parsing_elapsed)
                << " ns" << std::endl;
    }
  }

  {
    std::sort(latency_metrics_.begin(), latency_metrics_.end(),
              BenchmarkMetrics::CompareByHandlerCallingElapsed);
    std::cout << "Handler calling Elapsed: " << std::endl;
    for (auto& p : kPercentiles) {
      auto index = latency_metrics_.size() / 100 * p;

      std::cout << "\t" << p << "th percentile: "
                << absl::ToInt64Nanoseconds(
                       latency_metrics_.at(index).handler_calling_elapse)
                << " ns" << std::endl;
    }
  }
}

void RomaBenchmark::SendRequestBatch() {
  std::vector<InvocationSharedRequest<>> requests;
  for (auto i = 0; i < batch_size_; i++) {
    requests.push_back(code_obj_);
  }
  std::atomic<size_t> sent_request = 0;
  while (sent_request < requests_per_thread_) {
    while (!roma_service_
                ->BatchExecute(
                    requests,
                    std::bind(&RomaBenchmark::CallbackBatch, this,
                              std::placeholders::_1,
                              privacy_sandbox::server_common::Stopwatch()))
                .ok()) {
    }
    sent_request++;
  }
}

void RomaBenchmark::SendRequest() {
  std::atomic<size_t> sent_request = 0;
  while (sent_request < requests_per_thread_) {
    auto code_object = std::make_unique<InvocationSharedRequest<>>(code_obj_);
    // Retry Execute to dispatch code_obj until success.
    while (
        !roma_service_
             ->Execute(std::move(code_object),
                       std::bind(&RomaBenchmark::Callback, this,
                                 std::placeholders::_1,
                                 privacy_sandbox::server_common::Stopwatch()))
             .ok()) {
      // Recreate code_object and update start_time when request send failed.
      code_object = std::make_unique<InvocationSharedRequest<>>(code_obj_);
    }
    sent_request++;
  }
}

void RomaBenchmark::CallbackBatch(
    const std::vector<absl::StatusOr<ResponseObject>> resp_batch,
    privacy_sandbox::server_common::Stopwatch stopwatch) {
  for (auto resp : resp_batch) {
    if (!resp.ok()) {
      failed_requests_.fetch_add(1);
      return;
    }
  }

  success_requests_.fetch_add(1);
  BenchmarkMetrics metric;
  metric.total_execute_time = stopwatch.GetElapsedTime();
  latency_metrics_.at(metric_index_) = metric;
  metric_index_.fetch_add(1);
}

void RomaBenchmark::Callback(
    absl::StatusOr<ResponseObject> resp,
    privacy_sandbox::server_common::Stopwatch stopwatch) {
  if (!resp.ok()) {
    failed_requests_.fetch_add(1);
    return;
  }
  success_requests_.fetch_add(1);

  BenchmarkMetrics metric;
  metric.total_execute_time = stopwatch.GetElapsedTime();
  GetMetricFromResponse(resp.value(), metric);
  latency_metrics_.at(metric_index_) = metric;
  metric_index_.fetch_add(1);
}

}  // namespace google::scp::roma::benchmark
