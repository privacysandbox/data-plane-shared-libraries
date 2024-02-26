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

#ifndef PROXY_SOCKET_VENDOR_SERVER_H_
#define PROXY_SOCKET_VENDOR_SERVER_H_

#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include "client_session_pool.h"
#include "socket_types.h"

namespace google::scp::proxy {
class SocketVendorServer {
 public:
  SocketVendorServer(std::string sock_path, Endpoint proxy_endpoint,
                     size_t concurrency)
      : acceptor_(io_context_),
        sock_path_(std::move(sock_path)),
        proxy_endpoint_(proxy_endpoint),
        concurrency_(concurrency > 0 ? concurrency
                                     : std::thread::hardware_concurrency()) {}

  bool Init();
  void Run();
  void Stop();

 private:
  void StartAsyncAccept();

  boost::asio::io_context io_context_;
  Acceptor acceptor_;
  std::vector<std::thread> workers_;
  std::string sock_path_;
  Endpoint proxy_endpoint_;
  size_t concurrency_;
};
}  // namespace google::scp::proxy

#endif  // PROXY_SOCKET_VENDOR_SERVER_H_
