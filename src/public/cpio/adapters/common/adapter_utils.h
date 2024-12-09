/*
 * Copyright 2022 Google LLC
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

#ifndef PUBLIC_CPIO_ADAPTERS_COMMON_ADAPTER_UTILS_H_
#define PUBLIC_CPIO_ADAPTERS_COMMON_ADAPTER_UTILS_H_

#include <functional>
#include <memory>
#include <utility>

#include "src/core/interface/async_context.h"
#include "src/core/utils/error_utils.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::cpio {
/**
 * @brief Executes the async call.
 *
 * @tparam TRequest request type of the async call.
 * @tparam TResponse response type of the async call.
 * @param context async context.
 * @return core::ExecutionResult execution result.
 */
template <typename TRequest, typename TResponse>
void OnExecutionCallback(Callback<TResponse>& client_callback,
                         core::AsyncContext<TRequest, TResponse>& context) {
  if (!context.result.Successful()) {
    constexpr std::string_view kCpioClient = "CpioClient";
    SCP_ERROR_CONTEXT(kCpioClient, context, context.result,
                      "Failed to execute.");
    client_callback(core::utils::ConvertToPublicExecutionResult(context.result),
                    TResponse());
    return;
  }
  client_callback(core::utils::ConvertToPublicExecutionResult(context.result),
                  std::move(*context.response));
}

/**
 * @brief Executes the async call.
 *
 * @tparam TRequest request type of the async call.
 * @tparam TResponse response type of the async call.
 * @param context async context.
 * @return ExecutionResult or absl::Status
 */
template <typename TRequest, typename TResponse, typename Fn>
auto Execute(Fn&& func, TRequest request, Callback<TResponse> callback) {
  auto activity_id = core::common::Uuid::GenerateUuid();
  core::AsyncContext<TRequest, TResponse> context(
      std::make_shared<TRequest>(std::move(request)),
      absl::bind_front(OnExecutionCallback<TRequest, TResponse>,
                       std::move(callback)),
      activity_id, activity_id);
  if constexpr (std::is_invocable_r_v<core::ExecutionResult, Fn,
                                      decltype(context)&>) {
    return std::invoke(std::forward<Fn>(func), context);
  } else {
    static_assert(std::is_invocable_r_v<absl::Status, Fn, decltype(context)&>);
    return std::invoke(std::forward<Fn>(func), context);
  }
}
}  // namespace google::scp::cpio

#endif  // PUBLIC_CPIO_ADAPTERS_COMMON_ADAPTER_UTILS_H_
