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

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/util/duration.h"

namespace google::scp::roma::benchmark {
enum InputsType {
  // If the input type is simple string, a dummy string input of the set length
  // is generated.
  kSimpleString,
  // If the input type is nested Json string, a nested JSON string input of the
  // set depth will be generated.
  kNestedJsonString
};

struct TestConfiguration {
  // the type of the input.
  InputsType inputs_type;
  // the input payload size of the request
  size_t input_payload_in_byte;
  // the nested depth of the JSON type inputs.
  size_t input_json_nested_depth;
  // number of workers in ROMA
  size_t workers;
  // the queue size for each worker
  size_t queue_size;
  // how many threads used to send request to Roma
  size_t request_threads;
  // the number of requests for a batch request
  size_t batch_size;
  // the requests sent per thread
  size_t requests_per_thread;
  // JS source code for test. This only can be non-parameter JS code. If no
  // js_source_code provided, a simple Hello_World js code will be used for
  // testing.
  std::string js_source_code;
};

/**
 * @brief Construct a new Roma Benchmark Suit object
 *
 * @param test_configuration
 */
void RomaBenchmarkSuite(const TestConfiguration& test_configuration);

/**
 * @brief The benchmark metrics for each request.
 *
 */
struct BenchmarkMetrics {
  /// @brief The total time from when the request is sent to Roma to when the
  /// response is received. This includes the time to send the request, the time
  /// to process the request in the Roma worker sandbox, and the time to return
  /// the response.
  absl::Duration total_execute_time;

  /// @brief The time from when the request is sent into the Roma worker sandbox
  /// to when the response is received. This includes the time to parse the
  /// request, the time to process the request in the V8 sandbox, and the time
  /// to generate the response.
  absl::Duration sandbox_elapsed;

  /// @brief The time the request executes in the V8 sandbox.
  absl::Duration v8_elapsed;

  /// @brief The latency for the JS engine parses the JSON type of the request
  /// inputs.
  absl::Duration input_parsing_elapsed;

  /// @brief The latency for the JS engine to call the handler function from
  /// the request.
  absl::Duration handler_calling_elapse;

  static bool CompareByTotalExec(const BenchmarkMetrics& a,
                                 const BenchmarkMetrics& b) {
    return a.total_execute_time < b.total_execute_time;
  }

  static bool CompareBySandboxElapsed(const BenchmarkMetrics& a,
                                      const BenchmarkMetrics& b) {
    return a.sandbox_elapsed < b.sandbox_elapsed;
  }

  static bool CompareByV8Elapsed(const BenchmarkMetrics& a,
                                 const BenchmarkMetrics& b) {
    return a.v8_elapsed < b.v8_elapsed;
  }

  static bool CompareByInputsParsingElapsed(const BenchmarkMetrics& a,
                                            const BenchmarkMetrics& b) {
    return a.input_parsing_elapsed < b.input_parsing_elapsed;
  }

  static bool CompareByHandlerCallingElapsed(const BenchmarkMetrics& a,
                                             const BenchmarkMetrics& b) {
    return a.handler_calling_elapse < b.handler_calling_elapse;
  }

  static BenchmarkMetrics GetMeanMetrics(
      const std::vector<BenchmarkMetrics>& metrics);
};

/**
 * @brief Load the code object to Roma.
 *
 * @param code_string the string of JS source code.
 * @return Status
 */
absl::Status LoadCodeObject(
    google::scp::roma::sandbox::roma_service::RomaService<>& roma_service,
    std::string_view code_string);

class RomaBenchmark {
 public:
  /**
   * @brief Construct a new Roma Benchmark object. The product of threads and
   * requests_per_thread is the total number of requests to be tested.
   *
   * @param payload_size the invocation request payload size set by increasing
   * the input size.
   * @param batch_size the size of requests for each batch.
   * @param threads number of threads used to send request.
   * @param requests_per_thread number of requests sent by each thread.
   */
  explicit RomaBenchmark(
      std::unique_ptr<google::scp::roma::sandbox::roma_service::RomaService<>>
          roma_service,
      const InvocationSharedRequest<>& test_request, size_t batch_size,
      size_t threads, size_t requests_per_thread);

  ~RomaBenchmark();

  /**
   * @brief Run the benchmark test. This is blocking call, and the function only
   * return once all requests finished.
   *
   */
  void RunTest();

  /**
   * @brief Console test metrics. This function is only called after RunTest()
   * has completed.
   *
   */
  void ConsoleTestMetrics();

 private:
  void SendRequestBatch();

  void SendRequest();

  void CallbackBatch(
      const std::vector<absl::StatusOr<ResponseObject>> resp_batch,
      privacy_sandbox::server_common::Stopwatch stopwatch);

  void Callback(absl::StatusOr<ResponseObject> resp,
                privacy_sandbox::server_common::Stopwatch stopwatch);

  InvocationSharedRequest<> code_obj_;

  size_t threads_{0};
  size_t batch_size_{0};
  size_t requests_per_thread_{0};

  std::atomic<uint64_t> success_requests_{0};
  std::atomic<uint64_t> failed_requests_{0};
  std::atomic<uint64_t> metric_index_{0};
  std::vector<BenchmarkMetrics> latency_metrics_;
  absl::Duration elapsed_time_;
  std::unique_ptr<google::scp::roma::sandbox::roma_service::RomaService<>>
      roma_service_;
};
}  // namespace google::scp::roma::benchmark
