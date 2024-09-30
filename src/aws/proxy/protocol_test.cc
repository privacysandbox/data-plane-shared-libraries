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

#include "src/aws/proxy/protocol.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>

namespace google::scp::proxy {

TEST(ProtocolTest, FillAddrPortV4) {
  constexpr std::string_view kIpv4Addr = "12.34.56.78";
  constexpr uint16_t kPort = 0x1234;
  constexpr uint8_t kExpected[] = {0x01, 12, 34, 56, 78, 0x12, 0x34};
  ::sockaddr_in v4addr = {
      .sin_family = AF_INET,
      .sin_port = htons(kPort),
  };
  inet_aton(kIpv4Addr.data(), &v4addr.sin_addr);
  uint8_t buffer[64];
  EXPECT_EQ(FillAddrPort(buffer, reinterpret_cast<::sockaddr*>(&v4addr)),
            sizeof(kExpected));
  EXPECT_EQ(memcmp(buffer, kExpected, sizeof(kExpected)), 0);
}

TEST(ProtocolTest, FillAddrPortV6) {
  constexpr std::string_view kIpv6Addr =
      "1234:5678:90ab:cdef:1234:5678:90ab:cdef";
  constexpr uint16_t kPort = 0x1234;
  constexpr uint8_t kExpected[] = {
      0x04, 0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef, 0x12,
      0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef, 0x12, 0x34,
  };
  ::sockaddr_in6 v6addr = {
      .sin6_family = AF_INET6,
      .sin6_port = htons(kPort),
  };
  inet_pton(AF_INET6, kIpv6Addr.data(), &v6addr.sin6_addr);
  uint8_t buffer[64];
  EXPECT_EQ(FillAddrPort(buffer, reinterpret_cast<::sockaddr*>(&v6addr)),
            sizeof(kExpected));
  EXPECT_EQ(memcmp(buffer, kExpected, sizeof(kExpected)), 0);
}

}  // namespace google::scp::proxy
