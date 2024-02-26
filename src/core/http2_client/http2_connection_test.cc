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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdint.h>

#include <chrono>
#include <csignal>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <nghttp2/asio_http2_server.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "absl/synchronization/notification.h"
#include "src/core/async_executor/async_executor.h"
#include "src/core/async_executor/mock/mock_async_executor.h"
#include "src/core/http2_client/http2_client.h"
#include "src/core/http2_client/mock/mock_http_connection.h"
#include "src/core/interface/async_context.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using namespace nghttp2::asio_http2;          // NOLINT
using namespace nghttp2::asio_http2::server;  // NOLINT

using google::scp::core::AsyncExecutor;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::common::Uuid;
using google::scp::core::http2_client::mock::MockHttpConnection;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;

namespace google::scp::core {

TEST(HttpConnectionTest, SimpleRequest) {
  http2 server;
  boost::system::error_code ec;
  absl::Notification release_response;
  absl::Notification response_released;
  server.num_threads(1);
  server.handle("/test", [&](const request& req, const response& res) {
    release_response.WaitForNotification();

    res.write_head(200);
    res.end();

    response_released.Notify();
  });
  server.listen_and_serve(ec, "localhost", "0", true);

  AsyncExecutor async_executor(2, 20);
  MockHttpConnection connection(&async_executor, "localhost",
                                std::to_string(server.ports()[0]), false);

  ASSERT_SUCCESS(connection.Init());
  ASSERT_SUCCESS(connection.Run());

  std::vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_EQ(keys.size(), 0);

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->path =
      std::make_shared<std::string>("http://localhost/test");
  http_context.request->method = HttpMethod::GET;
  absl::Notification is_called;
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        is_called.Notify();
      };

  ExecutionResult execution_result = RetryExecutionResult(123);
  while (execution_result.status == ExecutionStatus::Retry) {
    execution_result = connection.Execute(http_context);
    usleep(1000);
  }

  ASSERT_SUCCESS(execution_result);

  while (keys.size() == 0) {
    connection.GetPendingNetworkCallbacks().Keys(keys);
    usleep(1000);
  }

  release_response.Notify();
  response_released.WaitForNotification();

  is_called.WaitForNotification();
  connection.Stop();
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_EQ(keys.size(), 0);

  server.stop();
  server.join();
}

TEST(HttpConnectionTest, CancelCallbacks) {
  http2 server;
  boost::system::error_code ec;
  absl::Notification release_response;
  server.num_threads(1);
  server.handle("/test", [&](const request& req, const response& res) {
    release_response.WaitForNotification();

    res.write_head(200);
    res.end();
  });
  server.listen_and_serve(ec, "localhost", "0", true);

  AsyncExecutor async_executor(2, 20);
  MockHttpConnection connection(&async_executor, "localhost",
                                std::to_string(server.ports()[0]), false);

  ASSERT_SUCCESS(connection.Init());
  ASSERT_SUCCESS(connection.Run());

  std::vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_EQ(keys.size(), 0);

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->path =
      std::make_shared<std::string>("http://localhost/test");
  http_context.request->method = HttpMethod::GET;
  bool is_called = false;
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        if (!is_called) {
          EXPECT_THAT(context.result,
                      ResultIs(FailureExecutionResult(
                          errors::SC_HTTP2_CLIENT_CONNECTION_DROPPED)));
          is_called = true;
        }
      };

  ExecutionResult execution_result = RetryExecutionResult(123);
  while (execution_result.status == ExecutionStatus::Retry) {
    execution_result = connection.Execute(http_context);
    usleep(1000);
  }

  ASSERT_SUCCESS(execution_result);

  while (keys.size() == 0) {
    connection.GetPendingNetworkCallbacks().Keys(keys);
    usleep(1000);
  }

  connection.CancelPendingCallbacks();
  EXPECT_TRUE(is_called);

  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_EQ(keys.size(), 0);

  release_response.Notify();
  connection.Stop();
  server.stop();
  server.join();
}

TEST(HttpConnectionTest, StopRemovesCallback) {
  http2 server;
  boost::system::error_code ec;
  absl::Notification release_response;
  server.num_threads(1);
  server.handle("/test", [&](const request& req, const response& res) {
    release_response.WaitForNotification();

    res.write_head(200);
    res.end();
  });
  server.listen_and_serve(ec, "localhost", "0", true);

  AsyncExecutor async_executor(2, 20);
  MockHttpConnection connection(&async_executor, "localhost",
                                std::to_string(server.ports()[0]), false);

  ASSERT_SUCCESS(connection.Init());
  ASSERT_SUCCESS(connection.Run());

  std::vector<Uuid> keys;
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_EQ(keys.size(), 0);

  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->path =
      std::make_shared<std::string>("http://localhost/test");
  http_context.request->method = HttpMethod::GET;
  bool is_called = false;
  http_context.callback =
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        if (!is_called) {
          EXPECT_THAT(context.result,
                      ResultIs(FailureExecutionResult(
                          errors::SC_HTTP2_CLIENT_CONNECTION_DROPPED)));
          is_called = true;
        }
      };

  ExecutionResult execution_result = RetryExecutionResult(123);
  while (execution_result.status == ExecutionStatus::Retry) {
    execution_result = connection.Execute(http_context);
    usleep(1000);
  }

  ASSERT_SUCCESS(execution_result);

  while (keys.size() == 0) {
    connection.GetPendingNetworkCallbacks().Keys(keys);
    usleep(1000);
  }

  connection.Stop();
  connection.GetPendingNetworkCallbacks().Keys(keys);
  EXPECT_EQ(keys.size(), 0);
  EXPECT_TRUE(is_called);

  release_response.Notify();
  connection.Stop();
  server.stop();
  server.join();
}

}  // namespace google::scp::core
