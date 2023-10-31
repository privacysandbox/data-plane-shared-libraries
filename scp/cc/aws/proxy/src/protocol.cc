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
static constexpr uint8_t kATYP_v4 = 0x01;
static constexpr uint8_t kATYP_v6 = 0x04;

size_t FillAddrPort(void* msg, const sockaddr* addr) {
  uint8_t* buf = reinterpret_cast<uint8_t*>(msg);
  switch (addr->sa_family) {
    case AF_INET: {
      buf[0] = kATYP_v4;
      const sockaddr_in* v4addr = reinterpret_cast<const sockaddr_in*>(addr);
      // V4 address is for sure 4 bytes.
      constexpr size_t sin_addr_size = sizeof(v4addr->sin_addr);
      constexpr size_t sin_port_size = sizeof(v4addr->sin_port);
      memcpy(&buf[1], &v4addr->sin_addr, sin_addr_size);
      // V4 address is for sure 2 bytes.
      memcpy(&buf[1 + sin_addr_size], &v4addr->sin_port, sin_port_size);
      constexpr size_t ret_size = 1UL + sin_addr_size + sin_port_size;
      static_assert(ret_size == 7);
      return ret_size;
    }
    case AF_INET6: {
      buf[0] = kATYP_v6;
      const sockaddr_in6* v6addr = reinterpret_cast<const sockaddr_in6*>(addr);
      constexpr size_t sin6_addr_size = sizeof(v6addr->sin6_addr);
      constexpr size_t sin6_port_size = sizeof(v6addr->sin6_port);
      // V6 address is for sure 16 bytes.
      memcpy(&buf[1], &v6addr->sin6_addr, sin6_addr_size);
      // V6 port is for sure 2 bytes.
      memcpy(&buf[1 + sin6_addr_size], &v6addr->sin6_port, sin6_port_size);
      constexpr size_t ret_size = 1UL + sin6_addr_size + sin6_port_size;
      static_assert(ret_size == 19);
      return ret_size;
    }
    default:
      return 0;
  }
}

// Construct a sockaddr of the parent instance by looking and env variables,
// or default if env not set.
sockaddr_vm GetProxyVsockAddr() {
  unsigned int cid = kDefaultParentCid;
  unsigned int port = kDefaultParentPort;
  EnvGetVal(kParentCidEnv, cid);
  EnvGetVal(kParentPortEnv, port);

  // memset(&addr, 0, sizeof(addr));
  sockaddr_vm addr = {
      .svm_family = AF_VSOCK,
      .svm_port = port,
      .svm_cid = cid,
  };
  return addr;
}

void EnvGetVal(std::string_view env_name, unsigned int& val) {
  char* val_str = getenv(env_name.data());
  if (val_str == nullptr) {
    return;
  }
  errno = 0;
  char* end;
  unsigned int v = strtoul(val_str, &end, 10);
  if (errno == 0 && v < UINT_MAX) {
    val = static_cast<unsigned int>(v);
  }
}
