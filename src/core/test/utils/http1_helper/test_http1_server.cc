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
#include "test_http1_server.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/core/test/utils/http1_helper/errors.h"
#include "src/public/core/interface/execution_result.h"

namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

namespace google::scp::core::test {
namespace {
void HandleErrorIfPresent(const boost::system::error_code& ec,
                          std::string stage) {
  if (ec) {
    std::cerr << stage << " failed: " << ec << std::endl;
    std::exit(EXIT_FAILURE);
  }
}
}  // namespace

// Uses the C socket library to bind to an unused port, close that socket then
// return that port number.
ExecutionResultOr<::in_port_t> GetUnusedPortNumber() {
  int sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    return FailureExecutionResult(
        errors::SC_TEST_HTTP1_SERVER_ERROR_GETTING_SOCKET);
  }
  ::sockaddr_in server_addr = {
      .sin_family = AF_INET,
      .sin_port = 0,
      .sin_addr{
          .s_addr = INADDR_ANY,
      },
  };
  ::socklen_t server_len = sizeof(server_addr);
  if (::bind(sockfd, reinterpret_cast<::sockaddr*>(&server_addr), server_len) <
      0) {
    return FailureExecutionResult(errors::SC_TEST_HTTP1_SERVER_ERROR_BINDING);
  }
  if (::getsockname(sockfd, reinterpret_cast<::sockaddr*>(&server_addr),
                    &server_len) < 0) {
    return FailureExecutionResult(
        errors::SC_TEST_HTTP1_SERVER_ERROR_GETTING_SOCKET_NAME);
  }
  ::close(sockfd);
  return server_addr.sin_port;
}

TestHttp1Server::TestHttp1Server()
    : response_body_(std::make_shared<std::string>()) {
  thread_ = std::thread([this]() {
    boost::asio::io_context ioc(/*concurrency_hint=*/1);
    tcp::endpoint ep(tcp::v4(), /*port=*/0);
    tcp::acceptor acceptor(ioc, ep);
    tcp::socket socket(ioc);
    port_number_ = acceptor.local_endpoint().port();

    // Handle connections until run_ is false.
    // Attempt to handle a request for 1 second - if run_ becomes false, stop
    // accepting requests and finish.
    // If run_ is still true, continue accepting requests.
    while (run_) {
      acceptor.async_accept(socket, [this, &socket](beast::error_code ec) {
        if (!ec) {
          ReadFromSocketAndWriteResponse(socket);
        } else {
          std::cerr << "accept failed: " << ec << std::endl;
          std::exit(EXIT_FAILURE);
        }
      });
      {
        absl::MutexLock lock(&has_run_mu_);
        has_run_ = true;
      }
      ioc.run_for(std::chrono::milliseconds(100));
      ioc.reset();
    }
  });

  // Ensure `thread_` runs at least once before constructor completes.
  absl::MutexLock lock(&has_run_mu_);
  has_run_mu_.Await(absl::Condition(&has_run_));
}

// Initiate the asynchronous operations associated with the connection.
void TestHttp1Server::ReadFromSocketAndWriteResponse(tcp::socket& socket) {
  // Clear any previous request's content.
  request_ = http::request<http::dynamic_body>();
  // The buffer for performing reads.
  beast::flat_buffer buffer{1024};
  beast::error_code ec;
  http::read(socket, buffer, request_, ec);
  HandleErrorIfPresent(ec, "read");

  // The response message.
  http::response<http::dynamic_body> response;
  response.version(request_.version());
  response.keep_alive(false);

  response.result(response_status_);

  for (const auto& [key, val] : response_headers_) {
    response.set(key, val);
  }
  if (response_body_ != nullptr) {
    beast::ostream(response.body()) << *response_body_;
  }
  response.content_length(response.body().size());

  http::write(socket, response, ec);
  HandleErrorIfPresent(ec, "write");

  socket.shutdown(tcp::socket::shutdown_send, ec);
  HandleErrorIfPresent(ec, "shutdown");
  socket.close(ec);
  HandleErrorIfPresent(ec, "close");
}

::in_port_t TestHttp1Server::PortNumber() const { return port_number_; }

std::string TestHttp1Server::GetPath() const {
  return "http://localhost:" + std::to_string(port_number_);
}

// Returns the request object that this server received.
const http::request<http::dynamic_body>& TestHttp1Server::Request() const {
  return request_;
}

std::string TestHttp1Server::RequestBody() const {
  return beast::buffers_to_string(request_.body().data());
}

void TestHttp1Server::SetResponseStatus(http::status status) {
  response_status_ = status;
}

void TestHttp1Server::SetResponseBody(
    const std::shared_ptr<std::string>& body) {
  response_body_ = body;
}

void TestHttp1Server::SetResponseHeaders(
    const absl::btree_multimap<std::string, std::string>& response_headers) {
  response_headers_ = response_headers;
}

TestHttp1Server::~TestHttp1Server() {
  // Indicate to thread_ that it should stop so we can safely destroy the
  // thread.
  run_ = false;
  thread_.join();
}

absl::btree_multimap<std::string, std::string> GetRequestHeadersMap(
    const http::request<http::dynamic_body>& request) {
  absl::btree_multimap<std::string, std::string> ret;
  for (const auto& header : request) {
    ret.insert(
        {std::string(header.name_string()), std::string(header.value())});
  }
  return ret;
}

}  // namespace google::scp::core::test
