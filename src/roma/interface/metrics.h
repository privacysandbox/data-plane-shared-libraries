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

#ifndef ROMA_INTERFACE_METRICS_H_
#define ROMA_INTERFACE_METRICS_H_

#include <string_view>

namespace google::scp::roma {
// Label for time taken to run code in the sandbox, called from outside the
// sandbox, meaning this includes serialization overhead. In milliseconds.
inline constexpr std::string_view kExecutionMetricDurationMs =
    "roma.execution.duration_ms";

// Label for the number of pending requests in the dispatcher queue at the time
// the request finishes executing.
inline constexpr std::string_view kExecutionMetricPendingRequestsCount =
    "roma.execution.pending_requests_count";

// Label for the ratio of workers actively processing requests at the time the
// request finishes executing.
inline constexpr std::string_view kExecutionMetricActiveWorkerRatio =
    "roma.execution.active_worker_ratio";

// Label for the time the request spent in the queue before being processed. In
// milliseconds.
inline constexpr std::string_view kExecutionMetricWaitTimeMs =
    "roma.execution.wait_time_ms";

// Label for time taken to run code E2E inside of the JS engine sandbox, meaning
// we skip the overhead for serializing data. Includes creation/fetching of
// V8::Context/V8::Isolate, i/o parsing, and JS handler call. In milliseconds.
inline constexpr std::string_view kExecutionMetricJsEngineCallDurationMs =
    "roma.execution.code_run_duration_ms";

// Label for time taken to parse the input in JS engine. In milliseconds.
inline constexpr std::string_view kInputParsingMetricJsEngineDurationMs =
    "roma.execution.json_input_parsing_duration_ms";

// Label for time taken to call handler function in JS engine. In milliseconds.
inline constexpr std::string_view kHandlerCallMetricJsEngineDurationMs =
    "roma.execution.js_engine_handler_call_duration_ms";

}  // namespace google::scp::roma

#endif  // ROMA_INTERFACE_METRICS_H_
