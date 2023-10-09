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
#include "glog/logging.h"
#include "proxy/src/config.h"
#include "proxy/src/proxy_server.h"

using google::scp::proxy::Config;
using google::scp::proxy::ProxyServer;

// Main loop - it all starts here...
int main(int argc, char* argv[]) {
  // The first thing we do is make sure that crashes will have a stacktrace
  // printed, with demangled symbols.
  absl::InitializeSymbolizer(argv[0]);
  {
    absl::FailureSignalHandlerOptions options;
    absl::InstallFailureSignalHandler(options);
  }
  google::InitGoogleLogging(argv[0]);
  // TODO(b/296559189): Writing to stdout rather than using GLog is necessary
  // here because socks5_test.py reads the TCP port that the test server is
  // running on from stdout.  Find a way to remove that dependency.
  std::cout << "Nitro Enclave Proxy (c) Google 2022." << std::endl
            << std::flush;

  {
    // Ignore SIGPIPE.
    struct sigaction act {};

    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, nullptr);
  }

  // Process command line parameters
  const Config config = Config::Parse(argc, argv);
  // Server server(config.socks5_port_, config.buffer_size_, config.vsock_);
  ProxyServer server(config);
  server.BindListen();
  // TODO(b/296559189): Writing to stdout rather than using GLog is necessary
  // here because socks5_test.py reads the TCP port that the test server is
  // running on from stdout.  Find a way to remove that dependency.
  std::cout << "Running on " << (config.vsock_ ? "VSOCK" : "TCP") << " port "
            << server.Port() << std::endl
            << std::flush;
  server.Run();

  LOG(FATAL) << "ERROR: A fatal error has occurred, terminating proxy instance";
  return 1;
}
