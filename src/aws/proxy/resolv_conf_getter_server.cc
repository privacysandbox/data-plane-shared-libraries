// Copyright 2023 Google LLC
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

#include <stdlib.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>

#include "absl/strings/str_cat.h"
#include "src/aws/proxy/resolv_conf_getter.grpc.pb.h"

namespace {

using privacy_sandbox::server_common::GetResolvConfRequest;
using privacy_sandbox::server_common::GetResolvConfResponse;
using privacy_sandbox::server_common::ResolvConfGetterService;

class ResolvConfGetterImpl final : public ResolvConfGetterService::Service {
 public:
  grpc::Status GetResolvConf(grpc::ServerContext* /*context*/,
                             const GetResolvConfRequest* /*request*/,
                             GetResolvConfResponse* response) {
    std::ifstream ifs("/etc/resolv.conf");
    response->mutable_content()->assign((std::istreambuf_iterator<char>(ifs)),
                                        (std::istreambuf_iterator<char>()));
    return grpc::Status::OK;
  }
};

}  // namespace

int main(int argc, char** argv) {
  if (argc > 1) {
    std::cerr << "Usage: resolv_conf_getter_server" << std::endl;
    return EXIT_FAILURE;
  }
  grpc::ServerBuilder builder;
  std::string server_address = "0.0.0.0:";
  if (char* port =
          std::getenv("PRIVACYSANDBOX_AWS_PROXY_RESOLV_CONF_SERVER_PORT");
      port != nullptr) {
    absl::StrAppend(&server_address, port);
  } else {
    absl::StrAppend(&server_address, 1600);
  }
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  ResolvConfGetterImpl service;
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}
