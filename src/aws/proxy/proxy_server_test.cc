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

#include "src/aws/proxy/proxy_server.h"

#include <gtest/gtest.h>

#include "src/aws/proxy/config.h"

using google::scp::proxy::Config;
using google::scp::proxy::ProxyServer;

namespace {

TEST(ProxyServerTest, VsockSmokeTest) {
  Config config;
  config.socks5_port = 0;  // This will have the OS pick a free port for us.
  config.vsock = true;

  ProxyServer server(config);
  ASSERT_EQ(server.Port(), 0);
  // An open port should be chosen here but vSock only works inside a hypervisor
  // and so for this unit test it'll continue to be zero.
  server.BindListen();
  ASSERT_EQ(server.Port(), 0);

  std::thread server_thread([&server] { server.Run(/*concurrency=*/1); });
  server.Stop();
  server_thread.join();

  EXPECT_EQ(server.Port(), 0);
}

TEST(ProxyServerTest, TcpSmokeTest) {
  Config config;
  config.socks5_port = 0;  // This will have the OS pick a free port for us.
  config.vsock = false;

  ProxyServer server(config);
  ASSERT_EQ(server.Port(), 0);
  // An open port should be chosen here:
  server.BindListen();
  ASSERT_NE(server.Port(), 0);

  std::thread server_thread([&server] { server.Run(/*concurrency=*/1); });
  server.Stop();
  server_thread.join();
}

}  // namespace
