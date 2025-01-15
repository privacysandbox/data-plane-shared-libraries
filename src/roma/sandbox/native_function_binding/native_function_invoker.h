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

#ifndef ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_NATIVE_FUNCTION_INVOKER_H_
#define ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_NATIVE_FUNCTION_INVOKER_H_

#include <memory>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "sandboxed_api/sandbox2/comms.h"
#include "src/roma/sandbox/native_function_binding/rpc_wrapper.pb.h"

namespace google::scp::roma::sandbox::native_function_binding {
/**
 * @brief Interface for a native function invoker, used to bridge between a
 * JS engine an a native C++ function. Generally, a C++ function would be
 * registered that can be called from a JS engine implementation, when a JS
 * function is called. This interface should be used to wrap specific
 * implementations. This is the handler of the call on the JS engine side.
 *
 */
class NativeFunctionInvoker {
 public:
  NativeFunctionInvoker() : fd_(-1) {}
  explicit NativeFunctionInvoker(int comms_fd);

  /**
   * @brief Invoke a native function linked to the given function invocation.
   * @param function_name is the name of the function to invoke. This is the
   * name that was used to register the function on the JS side.
   * @param[input] rpc_wrapper is a wrapper around the function binding context
   * and associated execution info needed for host process execution. The
   * function binding context is a two-way proto that contains the input to be
   * passed to the c++ function and the return value that is set by the c++
   * function and which is passed to the JS function as a return. The execution
   * info needed in the host process includes the associated invocation
   * request's id and uuid, and the name of the native function to call with
   * the function binding context in the host process.
   */
  virtual absl::Status Invoke(
      google::scp::roma::proto::RpcWrapper& rpc_wrapper_proto);

  virtual ~NativeFunctionInvoker() = default;

 private:
  std::unique_ptr<sandbox2::Comms> ipc_comms_;
  std::optional<int> fd_;
};
}  // namespace google::scp::roma::sandbox::native_function_binding

#endif  // ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_NATIVE_FUNCTION_INVOKER_H_
