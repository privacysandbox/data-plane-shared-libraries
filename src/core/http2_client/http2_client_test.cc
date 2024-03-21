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

#include "src/core/http2_client/http2_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdint.h>

#include <chrono>
#include <csignal>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <nghttp2/asio_http2_server.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "absl/functional/bind_front.h"
#include "absl/strings/numbers.h"
#include "absl/synchronization/notification.h"
#include "src/core/async_executor/async_executor.h"
#include "src/core/async_executor/mock/mock_async_executor.h"
#include "src/core/interface/async_context.h"
#include "src/core/test/utils/auto_init_run_stop.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using namespace nghttp2::asio_http2;          // NOLINT
using namespace nghttp2::asio_http2::server;  // NOLINT

using google::scp::core::AsyncExecutor;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TimeDuration;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::common::RetryStrategyOptions;
using google::scp::core::common::RetryStrategyType;
using google::scp::core::test::AutoInitRunStop;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using ::testing::StrEq;

namespace google::scp::core {
namespace {

constexpr TimeDuration kHttp2ReadTimeoutInSeconds = 10;

class RandomGenHandler : std::enable_shared_from_this<RandomGenHandler> {
 private:
  struct PrivateTag {};

 public:
  static std::shared_ptr<RandomGenHandler> Create(size_t length) {
    auto handler = std::make_shared<RandomGenHandler>(PrivateTag{}, length);
    return handler;
  }

  // Constructor uses private tag because `RandomGenHandler` manages its own
  // lifetime.
  explicit RandomGenHandler(PrivateTag, size_t length)
      : remaining_len_(length) {
    SHA256_Init(&sha256_ctx_);
  }

  ssize_t handle(uint8_t* data, size_t len, uint32_t* data_flags) {
    if (remaining_len_ == 0) {
      uint8_t hash[SHA256_DIGEST_LENGTH];
      SHA256_Final(hash, &sha256_ctx_);
      memcpy(data, hash, sizeof(hash));
      *data_flags |= NGHTTP2_DATA_FLAG_EOF;
      return sizeof(hash);
    }
    size_t to_generate = len < remaining_len_ ? len : remaining_len_;
    RAND_bytes(data, to_generate);
    SHA256_Update(&sha256_ctx_, data, to_generate);
    remaining_len_ -= to_generate;
    return to_generate;
  }

 private:
  SHA256_CTX sha256_ctx_;
  size_t remaining_len_;
};

class HttpServer {
 public:
  HttpServer(std::string address, std::string port, size_t num_threads)
      : address_(address), port_(port), num_threads_(num_threads) {}

  ~HttpServer() { server.join(); }

  void Run() {
    boost::system::error_code ec;

    server.num_threads(num_threads_);

    server.handle("/stop",
                  [this](const request& req, const response& res) { Stop(); });

    server.handle("/test", [](const request& req, const response& res) {
      res.write_head(200, {{"foo", {"bar"}}});
      res.end("hello, world\n");
    });

    server.handle(
        "/pingpong_query_param", [](const request& req, const response& res) {
          res.write_head(200, {{"query_param", {req.uri().raw_query.c_str()}}});
          res.end("hello, world\n");
        });

    server.handle("/random", [](const request& req, const response& res) {
      const auto& query = req.uri().raw_query;
      if (query.empty()) {
        res.write_head(400u);
        res.end();
        return;
      }
      std::vector<std::string> params;
      static auto predicate = [](std::string::value_type c) {
        return c == '=';
      };
      boost::split(params, query, predicate);
      if (params.size() != 2 || params[0] != "length") {
        res.write_head(400u);
        res.end();
        return;
      }
      size_t length = 0;
      if (!absl::SimpleAtoi(std::string_view(params[1]), &length) ||
          length == 0) {
        res.write_head(400u);
        res.end();
        return;
      }

      res.write_head(
          200u, {{"content-length",
                  {std::to_string(length + SHA256_DIGEST_LENGTH), false}}});
      std::shared_ptr<RandomGenHandler> handler =
          RandomGenHandler::Create(length);
      res.end(absl::bind_front(&RandomGenHandler::handle, handler));
    });

    server.listen_and_serve(ec, address_, port_, true);

    is_running_ = true;
  }

  void Stop() {
    if (is_running_) {
      is_running_ = false;
      server.stop();
    }
  }

  int PortInUse() { return server.ports()[0]; }

  http2 server;

 private:
  std::atomic<bool> is_running_{false};
  std::string address_;
  std::string port_;
  size_t num_threads_;
};

TEST(HttpClientTest, FailedToConnect) {
  auto request = std::make_shared<HttpRequest>();
  request->method = HttpMethod::GET;
  request->path = std::make_shared<std::string>("http://localhost.failed:8000");
  MockAsyncExecutor async_executor;
  HttpClient http_client(&async_executor);
  absl::Notification finished;
  AsyncContext<HttpRequest, HttpResponse> context(
      std::move(request),
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_THAT(
            context.result,
            ResultIs(FailureExecutionResult(
                errors::SC_DISPATCHER_NOT_ENOUGH_TIME_REMAINED_FOR_OPERATION)));
        finished.Notify();
      });

  ASSERT_SUCCESS(http_client.PerformRequest(context));
  finished.WaitForNotification();
  http_client.Stop();
}

class HttpClientTestII : public ::testing::Test {
 protected:
  void SetUp() override {
    server.emplace("localhost", "0", 1);
    server->Run();
    async_executor.emplace(2, 1000);
    auto options = HttpClientOptions(
        RetryStrategyOptions(RetryStrategyType::Exponential,
                             kDefaultRetryStrategyDelayInMs,
                             kDefaultRetryStrategyMaxRetries),
        kDefaultMaxConnectionsPerHost, kHttp2ReadTimeoutInSeconds);

    http_client.emplace(&*async_executor, std::move(options));
  }

  void TearDown() override {
    ASSERT_SUCCESS(http_client->Stop());
    server->Stop();
  }

  std::optional<HttpServer> server;
  std::optional<AsyncExecutor> async_executor;
  std::optional<HttpClient> http_client;
};

TEST_F(HttpClientTestII, Success) {
  auto request = std::make_shared<HttpRequest>();
  request->method = HttpMethod::GET;
  request->path = std::make_shared<std::string>(
      "http://localhost:" + std::to_string(server->PortInUse()) + "/test");
  std::promise<void> done;
  AsyncContext<HttpRequest, HttpResponse> context(
      std::move(request),
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        ASSERT_SUCCESS(context.result);
        const auto& bytes = *context.response->body.bytes;
        EXPECT_THAT(std::string(bytes.begin(), bytes.end()),
                    StrEq("hello, world\n"));
        done.set_value();
      });

  ASSERT_SUCCESS(http_client->PerformRequest(context));

  done.get_future().get();
}

TEST_F(HttpClientTestII, SingleQueryIsEscaped) {
  auto request = std::make_shared<HttpRequest>();
  request->method = HttpMethod::GET;
  request->path = std::make_shared<Uri>(
      "http://localhost:" + std::to_string(server->PortInUse()) +
      "/pingpong_query_param");
  request->query = std::make_shared<std::string>("foo=!@#$");

  absl::Notification finished;
  AsyncContext<HttpRequest, HttpResponse> context(
      std::move(request),
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        ASSERT_SUCCESS(context.result);
        auto query_param_it = context.response->headers->find("query_param");
        EXPECT_NE(query_param_it, context.response->headers->end());
        EXPECT_THAT(query_param_it->second, StrEq("foo=%21%40%23%24"));
        finished.Notify();
      });

  ASSERT_SUCCESS(http_client->PerformRequest(context));
  finished.WaitForNotification();
}

TEST_F(HttpClientTestII, MultiQueryIsEscaped) {
  auto request = std::make_shared<HttpRequest>();
  request->method = HttpMethod::GET;
  request->path = std::make_shared<Uri>(
      "http://localhost:" + std::to_string(server->PortInUse()) +
      "/pingpong_query_param");
  request->query = std::make_shared<std::string>("foo=!@#$&bar=%^()");

  absl::Notification finished;
  AsyncContext<HttpRequest, HttpResponse> context(
      std::move(request),
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        ASSERT_SUCCESS(context.result);
        auto query_param_it = context.response->headers->find("query_param");
        EXPECT_NE(query_param_it, context.response->headers->end());
        EXPECT_THAT(query_param_it->second,
                    StrEq("foo=%21%40%23%24&bar=%25%5E%28%29"));
        finished.Notify();
      });

  ASSERT_SUCCESS(http_client->PerformRequest(context));
  finished.WaitForNotification();
}

TEST_F(HttpClientTestII, FailedToGetResponse) {
  auto request = std::make_shared<HttpRequest>();
  // Get has no corresponding handler.
  request->path = std::make_shared<std::string>(
      "http://localhost:" + std::to_string(server->PortInUse()) + "/wrong");
  std::promise<void> done;
  AsyncContext<HttpRequest, HttpResponse> context(
      std::move(request),
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        errors::SC_HTTP2_CLIENT_HTTP_STATUS_NOT_FOUND)));
        done.set_value();
      });

  ASSERT_SUCCESS(http_client->PerformRequest(context));
  done.get_future().get();
}

TEST_F(HttpClientTestII, SequentialReuse) {
  auto request = std::make_shared<HttpRequest>();
  request->method = HttpMethod::GET;
  request->path = std::make_shared<std::string>(
      "http://localhost:" + std::to_string(server->PortInUse()) + "/test");

  for (int i = 0; i < 10; ++i) {
    std::promise<void> done;
    AsyncContext<HttpRequest, HttpResponse> context(
        std::move(request),
        [&](AsyncContext<HttpRequest, HttpResponse>& context) {
          ASSERT_SUCCESS(context.result);
          const auto& bytes = *context.response->body.bytes;
          EXPECT_THAT(std::string(bytes.begin(), bytes.end()),
                      StrEq("hello, world\n"));
          done.set_value();
        });
    ASSERT_SUCCESS(http_client->PerformRequest(context));
    done.get_future().get();
  }
}

TEST_F(HttpClientTestII, ConcurrentReuse) {
  auto request = std::make_shared<HttpRequest>();
  request->method = HttpMethod::GET;
  request->path = std::make_shared<std::string>(
      "http://localhost:" + std::to_string(server->PortInUse()) + "/test");

  std::vector<std::promise<void>> done;
  done.reserve(10);
  for (int i = 0; i < 10; ++i) {
    done.emplace_back();
    AsyncContext<HttpRequest, HttpResponse> context(
        std::move(request),
        [&, i](AsyncContext<HttpRequest, HttpResponse>& context) {
          ASSERT_SUCCESS(context.result);
          const auto& bytes = *context.response->body.bytes;
          EXPECT_THAT(std::string(bytes.begin(), bytes.end()),
                      StrEq("hello, world\n"));
          done[i].set_value();
        });
    ASSERT_SUCCESS(http_client->PerformRequest(context));
  }
  for (auto& p : done) {
    p.get_future().get();
  }
}

// Request /random?length=xxxx and verify the hash of the return.
TEST_F(HttpClientTestII, LargeData) {
  auto request = std::make_shared<HttpRequest>();
  size_t to_generate = 1048576UL;
  request->path = std::make_shared<std::string>(
      "http://localhost:" + std::to_string(server->PortInUse()) + "/random");
  request->query =
      std::make_shared<std::string>("length=" + std::to_string(to_generate));
  absl::Notification finished;
  AsyncContext<HttpRequest, HttpResponse> context(
      std::move(request),
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        ASSERT_SUCCESS(context.result);
        EXPECT_EQ(context.response->body.length,
                  1048576 + SHA256_DIGEST_LENGTH);
        uint8_t hash[SHA256_DIGEST_LENGTH];
        const auto* data = reinterpret_cast<const uint8_t*>(
            context.response->body.bytes->data());
        SHA256(data, to_generate, hash);
        auto ret = memcmp(hash, data + to_generate, SHA256_DIGEST_LENGTH);
        EXPECT_EQ(ret, 0);
        finished.Notify();
      });

  ASSERT_SUCCESS(http_client->PerformRequest(context));
  finished.WaitForNotification();
}

TEST_F(HttpClientTestII, ClientFinishesContextWhenServerIsStopped) {
  auto request = std::make_shared<HttpRequest>();
  request->method = HttpMethod::GET;

  // Make success http request.
  {
    request->path = std::make_shared<std::string>(
        "http://localhost:" + std::to_string(server->PortInUse()) + "/test");
    std::promise<void> done;
    AsyncContext<HttpRequest, HttpResponse> context(
        std::move(request),
        [&](AsyncContext<HttpRequest, HttpResponse>& context) {
          EXPECT_THAT(context.result, IsSuccessful());
          const auto& bytes = *context.response->body.bytes;
          EXPECT_THAT(std::string(bytes.begin(), bytes.end()),
                      "hello, world\n");
          done.set_value();
        });

    EXPECT_THAT(http_client->PerformRequest(context), IsSuccessful());
    done.get_future().get();
  }

  // Http context will be finished correctly even the http server stopped.
  {
    request->path = std::make_shared<std::string>(
        "http://localhost:" + std::to_string(server->PortInUse()) + "/stop");

    std::promise<void> done;
    AsyncContext<HttpRequest, HttpResponse> context(
        std::move(request),
        [&](AsyncContext<HttpRequest, HttpResponse>& context) {
          EXPECT_FALSE(context.result.Successful());
          done.set_value();
        });
    EXPECT_THAT(http_client->PerformRequest(context), IsSuccessful());
    done.get_future().get();
  }
}

}  // namespace
}  // namespace google::scp::core
