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

#include <sys/syscall.h>

#include <linux/audit.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "sandboxed_api/sandbox2/buffer.h"
#include "sandboxed_api/sandbox2/policy.h"
#include "sandboxed_api/sandbox2/policybuilder.h"
#include "src/roma/config/config.h"
#include "src/roma/logging/logging.h"
#include "src/roma/sandbox/worker_api/sapi/worker_params.pb.h"
#include "src/roma/sandbox/worker_api/sapi/worker_wrapper-sapi.sapi.h"

namespace google::scp::roma::sandbox::worker_api {

/**
 * @brief Class used as the API from the parent/controlling process to call into
 * a SAPI sandbox containing a roma worker.
 *
 */
class WorkerSandboxApi {
 public:
  enum class RetryStatus { kDoNotRetry, kRetry };

  /**
   * @brief Construct a new Worker Sandbox Api object.
   *
   * @param worker_engine The JS engine type used to build the worker.
   * @param require_preload Whether code preloading is required for this engine.
   * @param native_js_function_comms_fd Filed descriptor to be used for native
   * function calls through the sandbox.
   * @param native_js_function_names The names of the functions that should be
   * registered to be available in JS.
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
   */
  WorkerSandboxApi(
      bool require_preload, int native_js_function_comms_fd,
      const std::vector<std::string>& native_js_function_names,
      const std::string& server_address, size_t max_worker_virtual_memory_mb,
      size_t js_engine_initial_heap_size_mb,
      size_t js_engine_maximum_heap_size_mb,
      size_t js_engine_max_wasm_memory_number_of_pages,
      size_t sandbox_request_response_shared_buffer_size_mb,
      bool enable_sandbox_sharing_request_response_with_buffer_only);

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

  std::pair<absl::Status, RetryStatus> InternalRunCodeBufferShareOnly(
      ::worker_api::WorkerParamsProto& params);

  void CreateWorkerSapiSandbox();

  // Transfer the local FD into sandboxee and return the remote FD.
  int TransferFdAndGetRemoteFd(std::unique_ptr<::sapi::v::Fd> local_fd);

  /**
   * @brief Class to allow overwriting the policy for the SAPI sandbox.
   *
   */
  // See BUILD file for named library "WorkerWrapper" in the
  // sapi_library roma_worker_wrapper_lib-sapi target.
  class WorkerSapiSandbox : public WorkerWrapperSandbox {
   public:
    explicit WorkerSapiSandbox(
        uint64_t rlimit_as_bytes = 0,
        int roma_vlog_level = std::numeric_limits<int>::min())
        : rlimit_as_bytes_(rlimit_as_bytes),
          roma_vlog_level_(roma_vlog_level) {}

   protected:
    // Gets extra arguments to be passed to the sandboxee.
    void GetArgs(std::vector<std::string>* args) const override {
#ifdef ABSL_MIN_LOG_LEVEL
      // Gets ABSL_MIN_LOG_LEVEL value and pass it into sandbox.
      args->push_back(
          absl::StrCat("--stderrthreshold=", int64_t{ABSL_MIN_LOG_LEVEL}));
#endif
    }

   private:
    // Gets the environment variables passed to the sandboxee.
    void GetEnvs(std::vector<std::string>* envs) const override {
      // This comes from go/sapi sandbox GeEnvs() default setting.
      envs->push_back("GOOGLE_LOGTOSTDERR=1");

      if (roma_vlog_level_ >= 0) {
        // Sets the severity level of the displayed logs for ROMA_VLOG.
        envs->push_back(absl::StrCat(kRomaVlogLevel, "=", roma_vlog_level_));
      }
    }

    // Modify the sandbox policy executor object
    void ModifyExecutor(sandbox2::Executor* executor) override {
      if (rlimit_as_bytes_ > 0) {
        executor->limits()->set_rlimit_as(rlimit_as_bytes_);
      }
      // Ensure no core files are generated to avoid egression from the sandbox.
      executor->limits()->set_rlimit_core(0);
    }

    // Build a custom sandbox policy needed proper worker operation
    std::unique_ptr<sandbox2::Policy> ModifyPolicy(
        sandbox2::PolicyBuilder*) override {
      auto sandbox_policy =
          sandbox2::PolicyBuilder()
              .AllowRead()
              .AllowWrite()
              .AllowOpen()
              .AllowSystemMalloc()
              .AllowHandleSignals()
              .AllowExit()
              .AllowStat()
              .AllowTime()
              .AllowGetIDs()
              .AllowGetPIDs()
              .AllowReadlink()
              .AllowMmap()
              .AllowFork()
#ifdef UNDEFINED_BEHAVIOR_SANITIZER
              .AllowPipe()
              .AllowLlvmSanitizers()
#endif
              .AllowSyscall(__NR_tgkill)
              .AllowSyscall(__NR_recvmsg)
              .AllowSyscall(__NR_sendmsg)
              .AllowSyscall(__NR_lseek)
              .AllowSyscall(__NR_futex)
              .AllowSyscall(__NR_close)
              .AllowSyscall(__NR_nanosleep)
              .AllowSyscall(__NR_sched_getaffinity)
              .AllowSyscall(__NR_mprotect)
              .AllowSyscall(__NR_clone3)
              .AllowSyscall(__NR_rseq)
              .AllowSyscall(__NR_set_robust_list)
              .AllowSyscall(__NR_prctl)
              .AllowSyscall(__NR_uname)
              .AllowSyscall(__NR_pkey_alloc)
              .AllowSyscall(__NR_madvise)
              .AllowSyscall(__NR_ioctl)
              .AllowSyscall(__NR_prlimit64)
              //------------------------
              // These are to send RPC out of sandbox:
              .AllowSyscall(__NR_eventfd2)
              .AllowSyscall(__NR_epoll_create1)
              .AllowSyscall(__NR_epoll_ctl)
#ifdef __NR_epoll_wait
              .AllowSyscall(__NR_epoll_wait)
#else
              .AllowSyscall(__NR_epoll_pwait)
#endif
              .AllowSyscall(__NR_getrandom)
              .AllowSyscall(__NR_socket)
              .AllowSyscall(__NR_fcntl)
              .AllowSyscall(__NR_connect)
              .AllowSyscall(__NR_getsockname)
              .AllowSyscall(__NR_setsockopt)
              .AllowSyscall(__NR_shutdown)
              //------------------------
              // These are for TCMalloc:
              .AllowTcMalloc()
              .AllowSyscall(__NR_sched_getparam)
              .AllowSyscall(__NR_sched_getscheduler)
              .AllowSyscall(__NR_clock_nanosleep)
              .AllowSyscall(__NR_sched_yield)
              .AllowSyscall(__NR_rseq)
              //------------------------
              .AllowDynamicStartup()
              // TODO: b/296560366 - Enable namespaces in Roma.
              .DisableNamespaces()
              .CollectStacktracesOnViolation(false)
              .CollectStacktracesOnSignal(false)
              .CollectStacktracesOnTimeout(false)
              .CollectStacktracesOnKill(false)
              .CollectStacktracesOnExit(false);

      // Stack traces are only collected in DEBUG mode.
#ifndef NDEBUG
      sandbox_policy.CollectStacktracesOnViolation(true)
          .CollectStacktracesOnSignal(true)
          .CollectStacktracesOnTimeout(true)
          .CollectStacktracesOnKill(true)
          .CollectStacktracesOnExit(true);

      ROMA_VLOG(1) << "Enable stack trace collection in sapi sandbox";
#endif
      return sandbox_policy.BuildOrDie();
    }

    uint64_t rlimit_as_bytes_ = 0;
    int roma_vlog_level_;
  };

  std::unique_ptr<WorkerSapiSandbox> worker_sapi_sandbox_;
  // See BUILD file for named library "WorkerWrapper" in the
  // sapi_library roma_worker_wrapper_lib-sapi target.
  std::unique_ptr<WorkerWrapperApi> worker_wrapper_api_;

  bool require_preload_;
  int native_js_function_comms_fd_;
  std::vector<std::string> native_js_function_names_;
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
};
}  // namespace google::scp::roma::sandbox::worker_api

#endif  // ROMA_SANDBOX_WORKER_API_SAPI_WORKER_SANDBOX_API_H_
