/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_EXPERIMENTAL_ASHRUTI_ROMA_GVISOR_CONTAINER_GRPC_CLIENT_H_
#define SRC_EXPERIMENTAL_ASHRUTI_ROMA_GVISOR_CONTAINER_GRPC_CLIENT_H_

#include <memory>

#include <grpcpp/grpcpp.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/roma/gvisor/interface/roma_api.grpc.pb.h"

namespace privacy_sandbox::server_common::gvisor {

class RomaClient {
 public:
  explicit RomaClient(std::shared_ptr<grpc::Channel> channel)
      : stub_(RomaGvisorService::NewStub(channel)) {}

  absl::StatusOr<LoadBinaryResponse> LoadBinary(std::string_view code_str);

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  absl::StatusOr<ExecuteBinaryResponse> ExecuteBinary(
      const ExecuteBinaryRequest& request);

 private:
  std::unique_ptr<RomaGvisorService::Stub> stub_;
};
}  // namespace privacy_sandbox::server_common::gvisor

#endif  // SRC_EXPERIMENTAL_ASHRUTI_ROMA_GVISOR_CONTAINER_GRPC_CLIENT_H_
