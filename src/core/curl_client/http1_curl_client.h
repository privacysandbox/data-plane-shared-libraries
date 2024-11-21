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

#ifndef CORE_CURL_CLIENT_HTTP1_CURL_CLIENT_H_
#define CORE_CURL_CLIENT_HTTP1_CURL_CLIENT_H_

#include <memory>

#include "absl/time/time.h"
#include "src/core/common/operation_dispatcher/operation_dispatcher.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/interface/http_client_interface.h"
#include "src/public/core/interface/execution_result.h"

#include "error_codes.h"
#include "http1_curl_wrapper.h"

namespace google::scp::core {

/*! @copydoc HttpClientInterface
 *  This client is explicitly an HTTP1 client, not HTTP2.
 */
class Http1CurlClient : public HttpClientInterface {
 public:
  /**
   * @brief Construct a new CURL Client object
   *
   * @param async_executor an instance of the async executor.
   * @param retry_strategy_type retry strategy type.
   * @param time_duration_ms delay time duration in ms for http client retry
   * strategy.
   * @param total_retries total retry counts.
   */
  explicit Http1CurlClient(
      AsyncExecutorInterface* cpu_async_executor,
      AsyncExecutorInterface* io_async_executor,
      std::unique_ptr<Http1CurlWrapperProvider> curl_wrapper_provider =
          std::make_unique<Http1CurlWrapperProvider>(),
      common::RetryStrategyOptions retry_strategy_options =
          common::RetryStrategyOptions(common::RetryStrategyType::Exponential,
                                       kDefaultRetryStrategyDelayInMs,
                                       kDefaultRetryStrategyMaxRetries));

  ExecutionResult PerformRequest(
      AsyncContext<HttpRequest, HttpResponse>& http_context) noexcept override;

  ExecutionResult PerformRequest(
      AsyncContext<HttpRequest, HttpResponse>& http_context,
      const absl::Duration& timeout) noexcept override;

 private:
  std::unique_ptr<Http1CurlWrapperProvider> curl_wrapper_provider_;

  AsyncExecutorInterface* cpu_async_executor_;
  /// Operation dispatcher
  common::OperationDispatcher operation_dispatcher_;
};

}  // namespace google::scp::core

#endif  // CORE_CURL_CLIENT_HTTP1_CURL_CLIENT_H_
