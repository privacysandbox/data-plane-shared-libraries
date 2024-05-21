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

#include "protocol.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <string_view>

// For both request and response the ATYP is byte 3, followed by the address and
// port.
// Request:
//    +----+-----+-------+------+----------+----------+
//    |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
//    +----+-----+-------+------+----------+----------+
//    | 1  |  1  | X'00' |  1   | Variable |    2     |
//    +----+-----+-------+------+----------+----------+
// Response:
//    +----+-----+-------+------+----------+----------+
//    |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
//    +----+-----+-------+------+----------+----------+
//    | 1  |  1  | X'00' |  1   | Variable |    2     |
//    +----+-----+-------+------+----------+----------+

namespace google::scp::proxy {
namespace {
constexpr uint8_t kATYP_v4 = 0x01;
constexpr uint8_t kATYP_v6 = 0x04;
constexpr std::string_view kParentCidEnv = "PROXY_PARENT_CID";
constexpr std::string_view kParentPortEnv = "PROXY_PARENT_PORT";
constexpr uint32_t kDefaultParentCid = 3;
constexpr uint32_t kDefaultParentPort = 8888;
}  // namespace

size_t FillAddrPort(void* msg, const sockaddr* addr) {
  uint8_t* const buf = reinterpret_cast<uint8_t*>(msg);
  switch (addr->sa_family) {
    case AF_INET: {
      buf[0] = kATYP_v4;
      const sockaddr_in* v4addr = reinterpret_cast<const sockaddr_in*>(addr);
      // V4 address is for sure 4 bytes.
      constexpr size_t kSinAddrSize = sizeof(v4addr->sin_addr);
      constexpr size_t kSinPortSize = sizeof(v4addr->sin_port);
      memcpy(&buf[1], &v4addr->sin_addr, kSinAddrSize);
      // V4 address is for sure 2 bytes.
      memcpy(&buf[1 + kSinAddrSize], &v4addr->sin_port, kSinPortSize);
      constexpr size_t kRetSize = 1UL + kSinAddrSize + kSinPortSize;
      static_assert(kRetSize == 7);
      return kRetSize;
    }
    case AF_INET6: {
      buf[0] = kATYP_v6;
      const sockaddr_in6* v6addr = reinterpret_cast<const sockaddr_in6*>(addr);
      constexpr size_t kSin6AddrSize = sizeof(v6addr->sin6_addr);
      constexpr size_t kSin6PortSize = sizeof(v6addr->sin6_port);
      // V6 address is for sure 16 bytes.
      memcpy(&buf[1], &v6addr->sin6_addr, kSin6AddrSize);
      // V6 port is for sure 2 bytes.
      memcpy(&buf[1 + kSin6AddrSize], &v6addr->sin6_port, kSin6PortSize);
      constexpr size_t kRetSize = 1UL + kSin6AddrSize + kSin6PortSize;
      static_assert(kRetSize == 19);
      return kRetSize;
    }
    default:
      return 0;
  }
}

namespace {
bool EnvGetInt(std::string_view env_name, uint32_t* out) {
  const char* const str_val = getenv(env_name.data());
  if (str_val == nullptr) {
    return false;
  }

  // Reset `errno` value so we can check `strtoul` error in isolation.
  errno = 0;
  const uint64_t val = strtoul(str_val, nullptr, /*base=*/10);
  if (errno != 0 || val >= UINT_MAX) {
    return false;
  }
  *out = static_cast<uint32_t>(val);
  return true;
}
}  // namespace

// Construct a sockaddr of the parent instance by looking and env variables,
// or default if env not set.
sockaddr_vm GetProxyVsockAddr() {
  uint32_t cid;
  if (!EnvGetInt(kParentCidEnv, &cid)) {
    cid = kDefaultParentCid;
  }
  uint32_t port;
  if (!EnvGetInt(kParentPortEnv, &port)) {
    port = kDefaultParentPort;
  }

  struct sockaddr_vm addr = {
      .svm_family = AF_VSOCK,
      .svm_port = port,
      .svm_cid = cid,
  };
  return addr;
}
}  // namespace google::scp::proxy
