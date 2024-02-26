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
 *
 * Example command to run this (the grep is necessary to avoid noisy log
 * output):
 *
 * builders/tools/bazel-debian run \
 * //src/roma/sandbox/dispatcher:dispatcher_benchmark \
 * --test_output=all
 */

#include <benchmark/benchmark.h>

#include "absl/status/statusor.h"
#include "absl/synchronization/blocking_counter.h"
#include "src/core/async_executor/async_executor.h"
#include "src/core/test/utils/auto_init_run_stop.h"
#include "src/roma/interface/roma.h"
#include "src/roma/sandbox/dispatcher/dispatcher.h"
#include "src/roma/sandbox/worker_api/worker_api.h"
#include "src/roma/sandbox/worker_api/worker_api_sapi.h"
#include "src/roma/sandbox/worker_pool/worker_pool.h"
#include "src/roma/sandbox/worker_pool/worker_pool_api_sapi.h"

namespace {

using google::scp::core::AsyncExecutor;
using google::scp::core::test::AutoInitRunStop;
using google::scp::core::test::AutoInitRunStopStatus;
using google::scp::roma::CodeObject;
using google::scp::roma::InvocationStrRequest;
using google::scp::roma::ResponseObject;
using google::scp::roma::sandbox::dispatcher::Dispatcher;
using google::scp::roma::sandbox::worker_api::WorkerApi;
using google::scp::roma::sandbox::worker_api::WorkerApiSapi;
using google::scp::roma::sandbox::worker_api::WorkerApiSapiConfig;
using google::scp::roma::sandbox::worker_pool::WorkerPool;
using google::scp::roma::sandbox::worker_pool::WorkerPoolApiSapi;

WorkerApiSapiConfig CreateWorkerApiSapiConfig() {
  return WorkerApiSapiConfig{
      .js_engine_require_code_preload = true,
      .native_js_function_comms_fd = -1,
      .native_js_function_names = std::vector<std::string>(),
      .max_worker_virtual_memory_mb = 0,
      .sandbox_request_response_shared_buffer_size_mb = 0,
      .enable_sandbox_sharing_request_response_with_buffer_only = false,
  };
}

void BM_Dispatch(benchmark::State& state) {
  const int number_of_calls = state.range(0);

  // Note: queue_cap must be large enough to hold all of the queued tasks for
  // the benchmark.
  AsyncExecutor async_executor(/*thread_count=*/1,
                               /*queue_cap=*/100000);

  std::vector<WorkerApiSapiConfig> configs{CreateWorkerApiSapiConfig()};

  WorkerPoolApiSapi worker_pool(configs);
  AutoInitRunStop for_async_executor(async_executor);
  AutoInitRunStopStatus for_worker_pool(worker_pool);

  // Note: max_pending_requests must be large enough to hold all of the queued
  // tasks for the benchmark.
  Dispatcher dispatcher(&async_executor, &worker_pool,
                        /*max_pending_requests=*/100000);

  for (auto _ : state) {
    absl::BlockingCounter is_loading(number_of_calls);
    for (int i = 0; i < number_of_calls; ++i) {
      auto load_request = std::make_unique<CodeObject>();
      load_request->id = "id";
      load_request->version_string = "v1";
      load_request->js = R"(function test() { return 'Hello World'; })";

      ASSERT_TRUE(
          dispatcher
              .Dispatch(std::move(load_request),
                        [&is_loading](absl::StatusOr<ResponseObject> resp) {
                          ASSERT_TRUE(resp.ok());
                          is_loading.DecrementCount();
                        })
              .ok());
    }
    is_loading.Wait();
  }
}

}  // namespace

BENCHMARK(BM_Dispatch)->RangeMultiplier(10)->Range(1, 1000);

// Run the benchmark
BENCHMARK_MAIN();
