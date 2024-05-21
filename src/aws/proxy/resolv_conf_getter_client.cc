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

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "src/aws/proxy/resolv_conf_getter.grpc.pb.h"

ABSL_FLAG(int, port, 1600, "Port on which client and server communicate.");

namespace {

using privacy_sandbox::server_common::GetResolvConfRequest;
using privacy_sandbox::server_common::GetResolvConfResponse;
using privacy_sandbox::server_common::ResolvConfGetterService;

class Client {
 public:
  explicit Client(std::shared_ptr<grpc::Channel> channel)
      : stub_(ResolvConfGetterService::NewStub(std::move(channel))) {}

  absl::StatusOr<std::string> GetResolvConf() {
    grpc::ClientContext context;
    GetResolvConfResponse response;
    GetResolvConfRequest request;
    if (grpc::Status status =
            stub_->GetResolvConf(&context, request, &response);
        !status.ok()) {
      const grpc::StatusCode grpc_code = status.error_code();
      const absl::StatusCode absl_code = (grpc_code != grpc::DO_NOT_USE)
                                             ? absl::StatusCode(grpc_code)
                                             : absl::StatusCode::kUnknown;
      return absl::Status(absl_code, std::move(status).error_message());
    }
    return response.content();
  }

 private:
  std::unique_ptr<ResolvConfGetterService::Stub> stub_;
};

}  // namespace

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  Client client(
      grpc::CreateChannel(absl::StrCat("localhost:", absl::GetFlag(FLAGS_port)),
                          grpc::InsecureChannelCredentials()));
  const absl::StatusOr<std::string> content = client.GetResolvConf();
  if (!content.ok()) {
    std::cerr << content.status() << std::endl;
    return -1;
  }
  std::ofstream ofs("/etc/resolv.conf");
  ofs << R"resolv(; BEGIN Enclave configuration.
; These lines are added by `resolv_conf_getter_client`.
; use-vc forces use of TCP for DNS resolutions.
; See https://man7.org/linux/man-pages/man5/resolv.conf.5.html
options use-vc timeout:2 attempts:5
; END Enclave configuration.

)resolv";
  ofs << *content;
}
