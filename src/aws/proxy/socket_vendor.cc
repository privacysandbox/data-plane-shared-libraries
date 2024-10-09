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

#include <fcntl.h>
#include <signal.h>

#include <iostream>
#include <string>

#include "absl/flags/parse.h"
#include "absl/log/flags.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"

#include "protocol.h"
#include "socket_vendor_server.h"

using google::scp::proxy::Endpoint;
using google::scp::proxy::SocketVendorServer;

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  std::cout << "Nitro Enclave Proxy Socket Vendor (c) Google 2022."
            << std::endl;
  const auto lockfile =
      absl::StrCat(google::scp::proxy::kSocketVendorUdsPath, ".lock");
  const int lock_fd =
      open(lockfile.c_str(), O_CLOEXEC | O_CREAT | O_RDWR, S_IRWXU | S_IRGRP);
  if (lock_fd < 0) {
    LOG(ERROR) << "Cannot open lock file: " << lockfile
               << ", error: " << strerror(errno);
    return 1;
  }
  struct flock lock = {
      .l_type = F_WRLCK,
      .l_whence = SEEK_SET,
      .l_start = 0,
      .l_len = 1,
      .l_pid = 0,
  };
  if (const int lock_result = fcntl(lock_fd, F_SETLK, &lock); lock_result < 0) {
    LOG(ERROR) << "Cannot lock file " << lockfile
               << ", another socket vendor is probably running.\n"
                  "(If so, it is probably OK.)";
    return 1;
  }

  {
    // Ignore SIGPIPE.
    struct sigaction act = {
        .sa_handler = SIG_IGN,
    };

    sigaction(SIGPIPE, &act, nullptr);
  }

  auto addr = google::scp::proxy::GetProxyVsockAddr();
  Endpoint ep(&addr, sizeof(addr));

  SocketVendorServer server(
      /*sock_path=*/std::string(google::scp::proxy::kSocketVendorUdsPath),
      /*proxy_endpoint=*/ep,
      /*concurrency=*/4);
  if (!server.Init()) {
    return 1;
  }
  server.Run();

  LOG(FATAL) << "ERROR: A fatal error has occurred, terminating proxy instance";
  return 1;
}
