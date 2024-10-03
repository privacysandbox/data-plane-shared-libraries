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

#include <memory>

#include "absl/status/status.h"
#include "sandboxed_api/sandbox2/comms.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/native_function_binding/native_function_invoker.h"

using google::scp::roma::proto::RpcWrapper;

namespace {
constexpr int kBadFd = -1;
}

namespace google::scp::roma::sandbox::native_function_binding {
NativeFunctionInvoker::NativeFunctionInvoker(int comms_fd) {
  if (comms_fd != kBadFd) {
    ipc_comms_ = std::make_unique<sandbox2::Comms>(comms_fd);
  }
}

absl::Status NativeFunctionInvoker::Invoke(RpcWrapper& rpc_wrapper_proto) {
  if (!ipc_comms_) {
    return absl::FailedPreconditionError(
        "A call to invoke was made with an uninitialized comms object.");
  }

  auto sent = ipc_comms_->SendProtoBuf(rpc_wrapper_proto);
  if (!sent) {
    return absl::InternalError(
        "Could not send the call to the parent process.");
  }

  auto recv = ipc_comms_->RecvProtoBuf(&rpc_wrapper_proto);
  if (!recv) {
    return absl::InternalError(
        "Could not receive a response from the parent process.");
  }

  return absl::OkStatus();
}
}  // namespace google::scp::roma::sandbox::native_function_binding
