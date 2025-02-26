/*
 * Copyright 2024 Google LLC
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

#ifndef ROMA_SANDBOX_WORKER_API_SAPI_WORKER_SAPI_SANDBOX_H_
#define ROMA_SANDBOX_WORKER_API_SAPI_WORKER_SAPI_SANDBOX_H_

#include <sys/syscall.h>

#include <linux/audit.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "sandboxed_api/sandbox2/policy.h"
#include "sandboxed_api/sandbox2/policybuilder.h"
#include "src/roma/config/config.h"
#include "src/roma/logging/logging.h"
#include "src/roma/sandbox/worker_api/sapi/worker_wrapper-sapi.sapi.h"

namespace google::scp::roma::sandbox::worker_api {
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
      : rlimit_as_bytes_(rlimit_as_bytes), roma_vlog_level_(roma_vlog_level) {}

 protected:
  // Gets extra arguments to be passed to the sandboxee.
  void GetArgs(std::vector<std::string>* args) const override {
    absl::SetVLogLevel("policy", 0);
    absl::SetVLogLevel("monitor_ptrace", 0);
    absl::SetVLogLevel("sandbox", 0);
    absl::SetVLogLevel("var_abstract", 0);
    absl::SetVLogLevel("monitor_base", 0);
    absl::SetVLogLevel("executor", 0);

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
    auto sandbox_policy = sandbox2::PolicyBuilder()
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
                              .AllowSyscall(__NR_pkey_mprotect)
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

}  // namespace google::scp::roma::sandbox::worker_api

#endif  // ROMA_SANDBOX_WORKER_API_SAPI_WORKER_SAPI_SANDBOX_H_
