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

#ifndef CORE_CURL_CLIENT_MOCK_MOCK_CURL_CLIENT_H_
#define CORE_CURL_CLIENT_MOCK_MOCK_CURL_CLIENT_H_

#include <gmock/gmock.h>

#include "absl/time/time.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/http_client_interface.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::core::test {

class MockCurlClient : public HttpClientInterface {
 public:
  ExecutionResult PerformRequest(
      AsyncContext<HttpRequest, HttpResponse>& context,
      const absl::Duration& timeout) noexcept override {
    return PerformRequest(context);
  }

  MOCK_METHOD(ExecutionResult, PerformRequest,
              ((AsyncContext<HttpRequest, HttpResponse>&)),
              (override, noexcept));
};

}  // namespace google::scp::core::test

#endif  // CORE_CURL_CLIENT_MOCK_MOCK_CURL_CLIENT_H_
