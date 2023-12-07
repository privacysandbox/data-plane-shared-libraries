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

#ifndef ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_SRC_NATIVE_FUNCTION_HANDLER_SAPI_IPC_H_
#define ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_SRC_NATIVE_FUNCTION_HANDLER_SAPI_IPC_H_

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "core/interface/service_interface.h"
#include "roma/logging/src/logging.h"
#include "roma/sandbox/constants/constants.h"
#include "sandboxed_api/sandbox2/comms.h"
#include "scp/cc/roma/sandbox/native_function_binding/src/rpc_wrapper.pb.h"

#include "native_function_table.h"

using google::scp::roma::sandbox::constants::kRequestId;
using google::scp::roma::sandbox::constants::kRequestUuid;

inline constexpr std::string_view kFailedNativeHandlerExecution =
    "ROMA: Failed to execute the C++ function.";
inline constexpr std::string_view kCouldNotFindFunctionName =
    "ROMA: Could not find C++ function by name.";

namespace google::scp::roma::sandbox::native_function_binding {

template <typename TMetadata = google::scp::roma::DefaultMetadata>
class NativeFunctionHandlerSapiIpc {
 public:
  /**
   * @brief Construct a new Native Function Handler Sapi Ipc object
   *
   * @param function_table The function table object to look up function
   * bindings in.
   * @param local_fds The local file descriptors that we will use to listen for
   * function calls.
   * @param remote_fds The remote file descriptors. These are what the remote
   * process uses to send requests to this process.
   */
  NativeFunctionHandlerSapiIpc(NativeFunctionTable<TMetadata>* function_table,
                               const std::vector<int>& local_fds,
                               std::vector<int> remote_fds)
      : stop_(false),
        function_table_(function_table),
        remote_fds_(std::move(remote_fds)) {
    ipc_comms_.reserve(local_fds.size());
    for (const int local_fd : local_fds) {
      ipc_comms_.emplace_back(local_fd);
    }
  }

  core::ExecutionResult Run() noexcept {
    ROMA_VLOG(9) << "Calling native function handler";
    for (int i = 0; i < ipc_comms_.size(); i++) {
      function_handler_threads_.emplace_back([this, i] {
        while (true) {
          sandbox2::Comms& comms = ipc_comms_.at(i);
          proto::RpcWrapper wrapper_proto;

          // This unblocks once a call is issued from the other side
          const bool received = comms.RecvProtoBuf(&wrapper_proto);
          if (stop_.load()) {
            break;
          }
          if (!received) {
            continue;
          }

          auto io_proto = wrapper_proto.mutable_io_proto();
          const auto& invocation_req_uuid = wrapper_proto.request_uuid();

          // Get function name
          if (const auto function_name = wrapper_proto.function_name();
              !function_name.empty()) {
            if (FunctionBindingPayload<TMetadata> wrapper{
                    *io_proto, std::move(GetMetadata(invocation_req_uuid))};
                !function_table_->Call(function_name, wrapper).Successful()) {
              // If execution failed, add errors to the proto to return
              io_proto->mutable_errors()->Add(
                  std::string(kFailedNativeHandlerExecution));
              ROMA_VLOG(1) << kFailedNativeHandlerExecution;
            }
          } else {
            // If we can't find the function, add errors to the proto to return
            io_proto->mutable_errors()->Add(
                std::string(kCouldNotFindFunctionName));
            ROMA_VLOG(1) << kCouldNotFindFunctionName;
          }
          if (!comms.SendProtoBuf(wrapper_proto)) {
            continue;
          }
        }
      });
    }

    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Stop() noexcept {
    stop_.store(true);

    // We write to the comms object so that we can unblock the function binding
    // threads waiting on it.
    for (const int fd : remote_fds_) {
      sandbox2::Comms remote_comms(fd);
      proto::FunctionBindingIoProto io_proto;
      remote_comms.SendProtoBuf(io_proto);
    }
    // Wait for the function binding threads to stop before terminating the
    // comms object below.
    for (auto& t : function_handler_threads_) {
      if (t.joinable()) {
        t.join();
      }
    }
    for (sandbox2::Comms& c : ipc_comms_) {
      c.Terminate();
    }
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult StoreMetadata(std::string uuid,
                                      TMetadata metadata) noexcept
      ABSL_LOCKS_EXCLUDED(metadata_map_mutex_) {
    absl::MutexLock lock(&metadata_map_mutex_);
    metadata_.emplace(std::move(uuid), std::move(metadata));
    return core::SuccessExecutionResult();
  }

 private:
  std::atomic<bool> stop_;

  NativeFunctionTable<TMetadata>* function_table_;
  std::vector<std::thread> function_handler_threads_;
  std::vector<sandbox2::Comms> ipc_comms_;
  // We need the remote file descriptors to unblock the local ones when stopping
  std::vector<int> remote_fds_;

  TMetadata GetMetadata(std::string_view uuid) noexcept
      ABSL_LOCKS_EXCLUDED(metadata_map_mutex_) {
    absl::MutexLock lock(&metadata_map_mutex_);

    const auto metadata_it = metadata_.find(uuid);
    CHECK(metadata_it != metadata_.end()) << "Metadata could not be found";
    return metadata_it->second;
  }

  // Map of invocation request uuid to associated metadata.
  absl::flat_hash_map<std::string, TMetadata> metadata_
      ABSL_GUARDED_BY(metadata_map_mutex_);
  absl::Mutex metadata_map_mutex_;
};

}  // namespace google::scp::roma::sandbox::native_function_binding

#endif  // ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_SRC_NATIVE_FUNCTION_HANDLER_SAPI_IPC_H_
