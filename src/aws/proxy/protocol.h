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

#ifndef PROXY_PROTOCOL_H_
#define PROXY_PROTOCOL_H_

#include <netinet/ip.h>

#include <linux/vm_sockets.h>

#include <string_view>

namespace google::scp::proxy {

inline constexpr std::string_view kSocketVendorUdsPath =
    "/tmp/socket_vendor.sock";

// Given a request/response, fill the address and port. Returns the number of
// bytes copied into msg.
size_t FillAddrPort(void* msg, const sockaddr* addr);

// Construct a sockaddr of the parent instance by looking and env variables, or
// default if env not set.
sockaddr_vm GetProxyVsockAddr();

}  // namespace google::scp::proxy

#endif  // PROXY_PROTOCOL_H_
