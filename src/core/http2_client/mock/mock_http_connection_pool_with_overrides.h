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

#ifndef CORE_HTTP2_CLIENT_MOCK_MOCK_HTTP_CONNECTION_POOL_WITH_OVERRIDES_H_
#define CORE_HTTP2_CLIENT_MOCK_MOCK_HTTP_CONNECTION_POOL_WITH_OVERRIDES_H_

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "src/core/http2_client/http_connection_pool.h"
#include "src/public/core/test_execution_result_matchers.h"

namespace google::scp::core::http2_client::mock {
class MockHttpConnectionPool : public HttpConnectionPool {
 public:
  MockHttpConnectionPool(AsyncExecutorInterface* async_executor,
                         size_t max_connection_per_host)
      : HttpConnectionPool(async_executor, max_connection_per_host) {}

  std::shared_ptr<HttpConnection> CreateHttpConnection(
      std::string host, std::string service, bool is_https,
      TimeDuration http2_read_timeout_in_sec) override {
    if (create_connection_override_) {
      return create_connection_override_(host, service, is_https);
    }
    return HttpConnectionPool::CreateHttpConnection(host, service, is_https,
                                                    http2_read_timeout_in_sec);
  }

  absl::flat_hash_map<std::string, std::vector<std::shared_ptr<HttpConnection>>>
  GetConnectionsMap() {
    std::vector<std::string> keys;
    EXPECT_SUCCESS(connections_.Keys(keys));

    absl::flat_hash_map<std::string,
                        std::vector<std::shared_ptr<HttpConnection>>>
        connections;
    for (auto& key : keys) {
      std::shared_ptr<MockHttpConnectionPool::HttpConnectionPoolEntry> value;
      EXPECT_SUCCESS(connections_.Find(key, value));
      for (auto& http_connection : value->http_connections) {
        connections[key].push_back(http_connection);
      }
    }
    return connections;
  }

  void RecycleConnection(
      std::shared_ptr<HttpConnection>& connection) noexcept override {
    if (recycle_connection_override_) {
      return recycle_connection_override_(connection);
    }
    return HttpConnectionPool::RecycleConnection(connection);
  }

  std::function<std::shared_ptr<HttpConnection>(std::string, std::string, bool)>
      create_connection_override_;
  std::function<void(std::shared_ptr<HttpConnection>&)>
      recycle_connection_override_;
};
}  // namespace google::scp::core::http2_client::mock

#endif  // CORE_HTTP2_CLIENT_MOCK_MOCK_HTTP_CONNECTION_POOL_WITH_OVERRIDES_H_
