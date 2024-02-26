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

#ifndef ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_NATIVE_FUNCTION_INVOKER_SAPI_IPC_H_
#define ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_NATIVE_FUNCTION_INVOKER_SAPI_IPC_H_

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "sandboxed_api/sandbox2/comms.h"
#include "src/roma/sandbox/native_function_binding/rpc_wrapper.pb.h"

#include "native_function_invoker.h"

namespace google::scp::roma::sandbox::native_function_binding {
/**
 * @brief Native function invoker that uses SAPI IPC to "call" a function by
 * sending data over a socket.
 *
 */
class NativeFunctionInvokerSapiIpc : public NativeFunctionInvoker {
 public:
  explicit NativeFunctionInvokerSapiIpc(int comms_fd);

  absl::Status Invoke(
      google::scp::roma::proto::RpcWrapper& rpc_wrapper_proto) override;

 private:
  std::unique_ptr<sandbox2::Comms> ipc_comms_;
};
}  // namespace google::scp::roma::sandbox::native_function_binding

#endif  // ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_NATIVE_FUNCTION_INVOKER_SAPI_IPC_H_
