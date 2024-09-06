// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "message_router.h"

#include <memory>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "src/public/core/interface/execution_result.h"

#include "error_codes.h"

using google::protobuf::Any;

namespace google::scp::core {

ExecutionResult MessageRouter::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult MessageRouter::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult MessageRouter::Stop() noexcept {
  return SuccessExecutionResult();
}

void MessageRouter::OnMessageReceived(
    const std::shared_ptr<AsyncContext<Any, Any>>& context) noexcept {
  std::optional<AsyncAction> action;
  {
    absl::MutexLock lock(&mu_);
    if (const auto it = actions_.find(context->request->type_url());
        it != actions_.end()) {
      action = it->second;
    }
  }
  if (!action.has_value()) {
    context->Finish(FailureExecutionResult(
        errors::SC_MESSAGE_ROUTER_REQUEST_NOT_SUBSCRIBED));
  } else {
    (*action)(*context);
  }
}

ExecutionResult MessageRouter::Subscribe(std::string request_type,
                                         AsyncAction action) noexcept {
  absl::MutexLock lock(&mu_);
  if (!actions_.try_emplace(std::move(request_type), std::move(action))
           .second) {
    return FailureExecutionResult(
        errors::SC_MESSAGE_ROUTER_REQUEST_ALREADY_SUBSCRIBED);
  }
  return SuccessExecutionResult();
}
}  // namespace google::scp::core
