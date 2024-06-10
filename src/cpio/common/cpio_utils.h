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

#ifndef CPIO_COMMON_CPIO_UTILS_H_
#define CPIO_COMMON_CPIO_UTILS_H_

#include <memory>
#include <string>
#include <utility>

#include "src/core/interface/async_context.h"
#include "src/public/core/interface/execution_result.h"
#include "src/util/status_macro/status_macros.h"

namespace google::scp::cpio::common {

class CpioUtils {
 public:
  template <typename RequestT, typename ResponseT>
  static absl::Status AsyncToSync(
      const std::function<
          absl::Status(core::AsyncContext<RequestT, ResponseT>&)>& func,
      RequestT request, ResponseT& response) noexcept {
    std::promise<std::pair<core::ExecutionResult, std::shared_ptr<ResponseT>>>
        request_promise;
    core::AsyncContext<RequestT, ResponseT> context;
    context.request = std::make_shared<RequestT>(std::move(request));
    context.callback = [&](core::AsyncContext<RequestT, ResponseT>& outcome) {
      request_promise.set_value({outcome.result, outcome.response});
    };
    PS_RETURN_IF_ERROR(func(context));
    auto [result, out_response] = request_promise.get_future().get();
    if (!result.Successful()) {
      return absl::UnknownError(
          google::scp::core::errors::GetErrorMessage(result.status_code));
    }
    response = *std::move(out_response);
    return absl::OkStatus();
  }
};
}  // namespace google::scp::cpio::common

#endif  // CPIO_COMMON_CPIO_UTILS_H_
