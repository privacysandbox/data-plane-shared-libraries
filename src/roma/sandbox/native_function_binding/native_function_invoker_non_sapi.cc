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

#include <unistd.h>

#include <string>

#include "absl/status/status.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/native_function_binding/native_function_invoker.h"

using google::scp::roma::proto::RpcWrapper;

namespace {
constexpr int kBadFd = -1;
}

namespace google::scp::roma::sandbox::native_function_binding {
NativeFunctionInvoker::NativeFunctionInvoker(int comms_fd) : fd_(comms_fd) {}

absl::Status NativeFunctionInvoker::Invoke(RpcWrapper& rpc_wrapper_proto) {
  if (!fd_.has_value()) {
    return absl::FailedPreconditionError(
        "A call to invoke was made without a file descriptor.");
  }

  if (fd_ == kBadFd) {
    return absl::FailedPreconditionError(
        "A call to invoke was made with an invalid file descriptor.");
  }
  std::string serialized_proto = rpc_wrapper_proto.SerializeAsString();
  if (serialized_proto.empty()) {
    return absl::FailedPreconditionError(
        "A call to invoke was made with an invalid proto.");
  }

  ssize_t sentBytes =
      write(*fd_, serialized_proto.c_str(), serialized_proto.size());
  if (sentBytes == -1) {
    return absl::InternalError(
        "Could not send the call to the NativeFunctionHandler.");
  }

  // This unblocks once a call is issued from the other side
  char buffer[1024] = {0};
  ssize_t bytesRead = read(*fd_, buffer, sizeof(buffer) - 1);
  if (bytesRead == -1) {
    return absl::InternalError(
        "Could not receive a response from the NativeFunctionHandler.");
  }
  std::string serialized_response = std::string(buffer, bytesRead);
  rpc_wrapper_proto.ParseFromString(serialized_response);
  return absl::OkStatus();
}
}  // namespace google::scp::roma::sandbox::native_function_binding
