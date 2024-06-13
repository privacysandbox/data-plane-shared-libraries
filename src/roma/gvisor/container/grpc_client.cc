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

#include "src/roma/gvisor/container/grpc_client.h"

#include <filesystem>
#include <string>
#include <string_view>

#include <grpcpp/grpcpp.h>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/core/common/uuid/uuid.h"
#include "src/roma/gvisor/interface/roma_api.grpc.pb.h"
#include "src/util/status_macro/status_macros.h"
#include "src/util/status_macro/status_util.h"

namespace privacy_sandbox::server_common::gvisor {

namespace {
absl::Status CopyBinary(std::string_view src, std::string_view dest_dir,
                        std::string_view code_token) {
  std::filesystem::path dest = std::filesystem::path(dest_dir) / code_token;
  if (std::error_code ec;
      !std::filesystem::copy_file(std::filesystem::path(src), dest, ec)) {
    LOG(ERROR) << "Failed to copy " << src << " to " << dest << " with error "
               << ec.message() << std::endl;
    return absl::InternalError(absl::StrCat("Failed to copy ", src, " to ",
                                            dest.c_str(), " with error ",
                                            ec.message()));
  }
  return absl::OkStatus();
}
}  // namespace

absl::StatusOr<std::string> RomaClient::LoadBinary(std::string_view code_path) {
  ::google::scp::core::common::Uuid uuid =
      ::google::scp::core::common::Uuid::GenerateUuid();
  std::string code_token_str = ::google::scp::core::common::ToString(uuid);
  PS_RETURN_IF_ERROR(CopyBinary(code_path, prog_dir_, code_token_str));
  LoadBinaryRequest request;
  request.set_code_token(code_token_str);
  LoadBinaryResponse response;
  grpc::ClientContext context;
  if (grpc::Status status = stub_->LoadBinary(&context, request, &response);
      !status.ok()) {
    return privacy_sandbox::server_common::ToAbslStatus(status);
  }
  return code_token_str;
}

// Assembles the client's payload, sends it and presents the response back
// from the server.
absl::StatusOr<ExecuteBinaryResponse> RomaClient::ExecuteBinary(
    const ExecuteBinaryRequest& request) {
  ExecuteBinaryResponse response;
  // Context for the client. It could be used to convey extra information to
  // the server and/or tweak certain RPC behaviors.
  grpc::ClientContext context;
  if (grpc::Status status = stub_->ExecuteBinary(&context, request, &response);
      !status.ok()) {
    return privacy_sandbox::server_common::ToAbslStatus(status);
  }
  return response;
}
}  // namespace privacy_sandbox::server_common::gvisor
