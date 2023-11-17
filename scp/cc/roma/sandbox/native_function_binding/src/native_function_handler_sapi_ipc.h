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
#include <vector>

#include "core/interface/service_interface.h"
#include "sandboxed_api/sandbox2/comms.h"

#include "native_function_table.h"

namespace google::scp::roma::sandbox::native_function_binding {
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
  NativeFunctionHandlerSapiIpc(NativeFunctionTable* function_table,
                               const std::vector<int>&,
                               std::vector<int> remote_fds);

  core::ExecutionResult Init() noexcept;

  core::ExecutionResult Run() noexcept;

  core::ExecutionResult Stop() noexcept;

  core::ExecutionResult StoreMetadata(std::string uuid,
                                      TMetadata metadata) noexcept;

 private:
  std::atomic<bool> stop_;

  NativeFunctionTable* function_table_;
  std::vector<std::thread> function_handler_threads_;
  std::vector<sandbox2::Comms> ipc_comms_;
  // We need the remote file descriptors to unblock the local ones when stopping
  std::vector<int> remote_fds_;

  // Map of invocation request uuid to associated metadata.
  absl::flat_hash_map<std::string, TMetadata> metadata_;
};
}  // namespace google::scp::roma::sandbox::native_function_binding

#endif  // ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_SRC_NATIVE_FUNCTION_HANDLER_SAPI_IPC_H_
