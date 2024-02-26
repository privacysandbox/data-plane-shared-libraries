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

#include <signal.h>

#include <iostream>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/flags.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "src/aws/proxy/config.h"
#include "src/aws/proxy/proxy_server.h"

using google::scp::proxy::Config;
using google::scp::proxy::ProxyServer;

ABSL_FLAG(size_t, buffer_size, 65536, "Buffer size to use for internal logic.");
ABSL_FLAG(uint16_t, port, 8888, "Port on which socks5 server listens.");
ABSL_FLAG(bool, use_vsock, true,
          "Set to true (or 1) if proxy listens on VSOCK. Set to false "
          "(or 0) if proxy listens on TCP");

int main(int argc, char* argv[]) {
  // Process command line parameters
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();

  // The first thing we do is make sure that crashes will have a stacktrace
  // printed, with demangled symbols.
  absl::InitializeSymbolizer(argv[0]);
  {
    absl::FailureSignalHandlerOptions options;
    absl::InstallFailureSignalHandler(options);
  }

  // TODO(b/296559189): Writing to stdout rather than using GLog is necessary
  // here because socks5_test.py reads the TCP port that the test server is
  // running on from stdout.  Find a way to remove that dependency.
  std::cout << "Nitro Enclave Proxy (c) Google 2022." << std::endl;

  {
    // Ignore SIGPIPE.
    struct sigaction act {};

    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, nullptr);
  }

  // Process flags
  size_t buffer_size = absl::GetFlag(FLAGS_buffer_size);
  CHECK(buffer_size > 0) << "ERROR: Invalid buffer size: " << buffer_size;
  uint16_t port = absl::GetFlag(FLAGS_port);
  CHECK(port < UINT16_MAX) << "ERROR: Invalid port number: " << port;
  const Config config = Config{
      .buffer_size = buffer_size,
      .socks5_port = port,
      .vsock = absl::GetFlag(FLAGS_use_vsock),
  };
  // Server server(config.socks5_port, config.buffer_size, config.vsock);
  ProxyServer server(config);
  server.BindListen();
  // TODO(b/296559189): Writing to stdout rather than using GLog is necessary
  // here because socks5_test.py reads the TCP port that the test server is
  // running on from stdout.  Find a way to remove that dependency.
  std::cout << "Running on " << (config.vsock ? "VSOCK" : "TCP") << " port "
            << server.Port() << std::endl;
  server.Run();

  LOG(FATAL) << "ERROR: A fatal error has occurred, terminating proxy instance";
  return 1;
}
