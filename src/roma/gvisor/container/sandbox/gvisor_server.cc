// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "src/roma/gvisor/container/sandbox/grpc_server.h"
#include "src/roma/gvisor/container/sandbox/pool_manager.h"

ABSL_FLAG(std::string, server_socket, "/sockdir/abcd.sock",
          "Server socket for reaching Roma app API");
ABSL_FLAG(std::vector<std::string>, libs,
          std::vector<std::string>({"/lib", "/lib64", "/hostsockdir"}),
          "Mounts containing dependencies needed by the binary");
ABSL_FLAG(int, worker_pool_size, 10,
          "Size of pool of workers responsible for executing the binaries");
ABSL_FLAG(std::string, callback_socket, "/hostsockdir/xyzw.sock",
          "Server socket for reaching host callback server");
ABSL_FLAG(std::string, prog_dir, "/progdir",
          "Directory mounted into the sandbox containing untrusted binaries");

int main(int argc, char* argv[]) {
  std::vector<char*> args = absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  if (args.empty()) {
    LOG(ERROR) << "No args.";
  }
  std::string server_socket = absl::GetFlag(FLAGS_server_socket);
  if (server_socket.empty()) {
    LOG(ERROR) << "No server socket provided.";
  }
  std::string prog_dir = absl::GetFlag(FLAGS_prog_dir);

  std::vector<std::string> mounts = absl::GetFlag(FLAGS_libs);
  mounts.push_back(prog_dir);
  const std::string callback_socket = absl::GetFlag(FLAGS_callback_socket);
  privacy_sandbox::server_common::gvisor::RomaGvisorPoolManager pool_manager(
      absl::GetFlag(FLAGS_worker_pool_size), mounts, prog_dir, callback_socket);
  privacy_sandbox::server_common::gvisor::RomaGvisorGrpcServer grpc_server;
  grpc_server.Run(server_socket, &pool_manager);
  std::filesystem::remove_all(prog_dir);
  return 0;
}
