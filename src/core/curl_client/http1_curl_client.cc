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
#include "http1_curl_client.h"

#include <string_view>
#include <utility>

#include "absl/time/time.h"
#include "src/public/core/interface/execution_result.h"

#include "http1_curl_wrapper.h"

using google::scp::core::common::RetryStrategy;
using google::scp::core::common::RetryStrategyType;

namespace {
constexpr std::string_view kHttp1CurlClient = "Http1CurlClient";
}

namespace google::scp::core {

Http1CurlClient::Http1CurlClient(
    AsyncExecutorInterface* cpu_async_executor,
    AsyncExecutorInterface* io_async_executor,
    std::unique_ptr<Http1CurlWrapperProvider> curl_wrapper_provider,
    common::RetryStrategyOptions retry_strategy_options)
    : curl_wrapper_provider_(std::move(curl_wrapper_provider)),
      cpu_async_executor_(cpu_async_executor),
      operation_dispatcher_(io_async_executor,
                            RetryStrategy(retry_strategy_options)) {}

ExecutionResult Http1CurlClient::PerformRequest(
    AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept {
  return PerformRequest(http_context, google::scp::core::kHttpRequestTimeout);
}

ExecutionResult Http1CurlClient::PerformRequest(
    AsyncContext<HttpRequest, HttpResponse>& http_context,
    const absl::Duration& timeout) noexcept {
  auto wrapper_or = curl_wrapper_provider_->MakeWrapper();
  RETURN_IF_FAILURE(wrapper_or.result());
  operation_dispatcher_.Dispatch<AsyncContext<HttpRequest, HttpResponse>>(
      http_context,
      [this, &timeout, wrapper = wrapper_or->get()](auto& http_context) {
        auto response_or =
            wrapper->PerformRequest(*http_context.request, timeout);
        if (!response_or.Successful()) {
          http_context.result = response_or.result();
          SCP_ERROR_CONTEXT(kHttp1CurlClient, http_context, http_context.result,
                            "wrapper PerformRequest failed.");
          return response_or.result();
        }

        http_context.response =
            std::make_shared<HttpResponse>(std::move(*response_or));

        FinishContext(SuccessExecutionResult(), http_context,
                      *cpu_async_executor_);

        return SuccessExecutionResult();
      });
  return SuccessExecutionResult();
}

}  // namespace google::scp::core
