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

#include "native_function_handler_sapi_ipc.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "roma/sandbox/constants/constants.h"
#include "scp/cc/roma/sandbox/native_function_binding/src/rpc_wrapper.pb.h"

using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::roma::sandbox::constants::kRequestId;
using google::scp::roma::sandbox::constants::kRequestUuid;

static constexpr char kFailedNativeHandlerExecution[] =
    "ROMA: Failed to execute the C++ function.";
static constexpr char kCouldNotFindFunctionName[] =
    "ROMA: Could not find C++ function by name.";

namespace google::scp::roma::sandbox::native_function_binding {

NativeFunctionHandlerSapiIpc::NativeFunctionHandlerSapiIpc(
    NativeFunctionTable* function_table, const std::vector<int>& local_fds,
    std::vector<int> remote_fds)
    : stop_(false),
      function_table_(function_table),
      remote_fds_(std::move(remote_fds)) {
  ipc_comms_.reserve(local_fds.size());
  for (const int local_fd : local_fds) {
    ipc_comms_.emplace_back(local_fd);
  }
}

ExecutionResult NativeFunctionHandlerSapiIpc::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult NativeFunctionHandlerSapiIpc::Run() noexcept {
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

        TMetadata metadata;
        auto io_proto = wrapper_proto.mutable_io_proto();
        const auto& invocation_req_uuid = wrapper_proto.request_uuid();
        if (const auto& metadata_it = metadata_.find(invocation_req_uuid);
            metadata_it != metadata_.end()) {
          metadata = metadata_it->second;
          metadata_.erase(metadata_it);
        }

        // Get function name
        if (const auto function_name = wrapper_proto.function_name();
            !function_name.empty()) {
          if (FunctionBindingPayload wrapper{*io_proto, metadata};
              !function_table_->Call(function_name, wrapper).Successful()) {
            // If execution failed, add errors to the proto to return
            io_proto->mutable_errors()->Add(kFailedNativeHandlerExecution);
          }
        } else {
          // If we can't find the function, add errors to the proto to return
          io_proto->mutable_errors()->Add(kCouldNotFindFunctionName);
        }
        if (!comms.SendProtoBuf(wrapper_proto)) {
          continue;
        }
      }
    });
  }

  return SuccessExecutionResult();
}

ExecutionResult NativeFunctionHandlerSapiIpc::StoreMetadata(
    std::string uuid, TMetadata metadata) noexcept {
  metadata_.emplace(std::move(uuid), std::move(metadata));
  return SuccessExecutionResult();
}

ExecutionResult NativeFunctionHandlerSapiIpc::Stop() noexcept {
  stop_.store(true);

  // We write to the comms object so that we can unblock the function binding
  // threads waiting on it.
  for (const int fd : remote_fds_) {
    sandbox2::Comms remote_comms(fd);
    proto::FunctionBindingIoProto io_proto;
    remote_comms.SendProtoBuf(io_proto);
  }
  // Wait for the function binding threads to stop before terminating the comms
  // object below.
  for (auto& t : function_handler_threads_) {
    if (t.joinable()) {
      t.join();
    }
  }
  for (sandbox2::Comms& c : ipc_comms_) {
    c.Terminate();
  }
  return SuccessExecutionResult();
}

}  // namespace google::scp::roma::sandbox::native_function_binding
