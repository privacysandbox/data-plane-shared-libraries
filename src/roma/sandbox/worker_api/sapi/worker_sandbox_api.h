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

#ifndef ROMA_SANDBOX_WORKER_API_SAPI_WORKER_SANDBOX_API_H_
#define ROMA_SANDBOX_WORKER_API_SAPI_WORKER_SANDBOX_API_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "sandboxed_api/sandbox2/buffer.h"
#include "src/roma/sandbox/worker_api/sapi/utils.h"
#include "src/roma/sandbox/worker_api/sapi/worker_params.pb.h"
#include "src/roma/sandbox/worker_api/sapi/worker_wrapper.h"

namespace google::scp::roma::sandbox::worker_api {

/**
 * @brief Class used as the API from the parent/controlling process to call into
 * a SAPI sandbox containing a roma worker.
 *
 */
class WorkerSandboxApi {
 public:
  /**
   * @brief Construct a new Worker Sandbox Api object.
   *
   * @param worker_engine The JS engine type used to build the worker.
   * @param require_preload Whether code preloading is required for this engine.
   * @param native_js_function_comms_fd Filed descriptor to be used for native
   * function calls through the sandbox.
   * @param native_js_function_names The names of the functions that should be
   * registered to be available in JS.
   * @param rpc_method_names The names of the rpc methods for registered
   * services. Will allow clients to invoke services via gRPC in UDF
   * @param server_address The address of the gRPC server in the host process
   * for native function calls.
   * @param max_worker_virtual_memory_mb The maximum amount of virtual memory in
   * MB that the worker process is allowed to use.
   * @param js_engine_initial_heap_size_mb The initial heap size in MB for the
   * JS engine.
   * @param js_engine_maximum_heap_size_mb The maximum heap size in MB for the
   * JS engine.
   * @param js_engine_max_wasm_memory_number_of_pages The maximum number of WASM
   * pages. Each page is 64KiB. Max 65536 pages (4GiB).
   * @param sandbox_request_response_shared_buffer_size_mb The size of the
   * Buffer in megabytes (MB). If the input value is equal to or less than zero,
   * the default value of 1MB will be used.
   * @param v8_flags List of flags to pass into v8. (Ex. {"--FLAG_1",
   * "--FLAG_2"})
   * @param enable_profilers Enable the V8 CPU and Heap Profilers
   * @param logging_function_set Whether a logging function has been registered
   * in Roma
   * @param disable_udf_stacktraces_in_response Whether UDF stacktrace should be
   * returned in response
   */
  WorkerSandboxApi(
      bool require_preload, int native_js_function_comms_fd,
      const std::vector<std::string>& native_js_function_names,
      const std::vector<std::string>& rpc_method_names,
      const std::string& server_address, size_t max_worker_virtual_memory_mb,
      size_t js_engine_initial_heap_size_mb,
      size_t js_engine_maximum_heap_size_mb,
      size_t js_engine_max_wasm_memory_number_of_pages,
      size_t sandbox_request_response_shared_buffer_size_mb,
      bool enable_sandbox_sharing_request_response_with_buffer_only,
      const std::vector<std::string>& v8_flags, bool enable_profilers,
      bool logging_function_set, bool disable_udf_stacktraces_in_response);

  absl::Status Init();

  absl::Status Run();

  absl::Status Stop();

  /**
   * @brief Send a request to run code to a worker running within a sandbox.
   *
   * @param params Proto representing a request to the worker.
   * @return Retry status and execution status
   */
  std::pair<absl::Status, RetryStatus> RunCode(
      ::worker_api::WorkerParamsProto& params);

  void Terminate();

 protected:
  std::pair<absl::Status, RetryStatus> InternalRunCode(
      ::worker_api::WorkerParamsProto& params);

  std::pair<absl::Status, RetryStatus> InternalRunCodeDebug(
      ::worker_api::WorkerParamsProto& params);

  std::pair<absl::Status, RetryStatus> InternalRunCodeBufferShareOnly(
      ::worker_api::WorkerParamsProto& params);

  std::unique_ptr<WorkerSapiSandbox> worker_sapi_sandbox_;
  std::unique_ptr<WorkerWrapper> worker_wrapper_;

  bool require_preload_;
  int native_js_function_comms_fd_;
  std::vector<std::string> native_js_function_names_;
  std::vector<std::string> rpc_method_names_;
  std::string server_address_;
  size_t max_worker_virtual_memory_mb_;
  size_t js_engine_initial_heap_size_mb_;
  size_t js_engine_maximum_heap_size_mb_;
  size_t js_engine_max_wasm_memory_number_of_pages_;

  // the pointer of the data shared sandbox2::Buffer which is used to share
  // input and output between the host process and the sandboxee.
  std::unique_ptr<sandbox2::Buffer> sandbox_data_shared_buffer_ptr_;
  // The capacity size of the Buffer in bytes.
  size_t request_and_response_data_buffer_size_bytes_;
  const bool enable_sandbox_sharing_request_response_with_buffer_only_;
  std::vector<std::string> v8_flags_;
  const bool enable_profilers_;
  const bool logging_function_set_;
  const bool disable_udf_stacktraces_in_response_;
};
}  // namespace google::scp::roma::sandbox::worker_api

#endif  // ROMA_SANDBOX_WORKER_API_SAPI_WORKER_SANDBOX_API_H_
