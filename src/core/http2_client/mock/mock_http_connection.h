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

#ifndef CORE_HTTP2_CLIENT_MOCK_MOCK_HTTP_CONNECTION_H_
#define CORE_HTTP2_CLIENT_MOCK_MOCK_HTTP_CONNECTION_H_

#include <memory>
#include <string>
#include <utility>

#include "src/core/http2_client/http_connection.h"

namespace google::scp::core::http2_client::mock {
class MockHttpConnection : public HttpConnection {
 public:
  MockHttpConnection(AsyncExecutorInterface* async_executor, std::string host,
                     std::string service, bool is_https)
      : HttpConnection(async_executor, std::move(host), std::move(service),
                       is_https) {}

  void CancelPendingCallbacks() noexcept {
    HttpConnection::CancelPendingCallbacks();
  }

  void SetIsDropped() { is_dropped_ = true; }

  void SetIsNotDropped() { is_dropped_ = false; }

  void SetIsNotReady() { is_ready_ = false; }

  void SetIsReady() { is_ready_ = true; }

  auto& GetPendingNetworkCallbacks() { return pending_network_calls_; }
};
}  // namespace google::scp::core::http2_client::mock

#endif  // CORE_HTTP2_CLIENT_MOCK_MOCK_HTTP_CONNECTION_H_
