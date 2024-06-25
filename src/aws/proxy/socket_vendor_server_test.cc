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

#include "src/aws/proxy/socket_vendor_server.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iostream>

#include "src/aws/proxy/socket_types.h"

namespace {

using google::scp::proxy::Endpoint;
using google::scp::proxy::SocketVendorServer;

TEST(SocketVendorServerTest, EmptyPath) {
  Endpoint proxy_endpoint;
  SocketVendorServer server(/*sock_path=*/"", proxy_endpoint,
                            /*concurrency=*/1);
  EXPECT_FALSE(server.Init());
}

TEST(SocketVendorServerTest, NonExistentSocketFile) {
  const std::filesystem::path sock_path =
      std::filesystem::temp_directory_path() / "foo_sock.file";
  Endpoint proxy_endpoint;
  SocketVendorServer server(sock_path.string(), proxy_endpoint,
                            /*concurrency=*/1);
  EXPECT_TRUE(server.Init());
}

TEST(SocketVendorServerTest, NonExistentSocketDir) {
  const std::filesystem::path sock_path =
      std::filesystem::path("/bar") / "foo" / "sock.file";
  Endpoint proxy_endpoint;
  SocketVendorServer server(sock_path.string(), proxy_endpoint,
                            /*concurrency=*/1);
  EXPECT_FALSE(server.Init());
}

TEST(SocketVendorServerTest, InitSuccess) {
  const std::filesystem::path sock_path =
      std::filesystem::temp_directory_path() / "sock.file";
  Endpoint proxy_endpoint;
  SocketVendorServer server(sock_path.string(), proxy_endpoint,
                            /*concurrency=*/1);
  EXPECT_TRUE(server.Init());
}

TEST(SocketVendorServerTest, RunStop) {
  const std::filesystem::path sock_path =
      std::filesystem::temp_directory_path() / "foo" / "sock.file";
  // Ensure the directory exists
  std::filesystem::create_directories(sock_path.parent_path());
  Endpoint proxy_endpoint;
  SocketVendorServer server(sock_path.string(), proxy_endpoint,
                            /*concurrency=*/1);
  ASSERT_TRUE(server.Init());
  std::thread server_thread([&server] { server.Run(); });
  server.Stop();
  server_thread.join();
}

}  // namespace
