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

#ifndef SRC_EXPERIMENTAL_ASHRUTI_ROMA_GVISOR_INTERFACE_ROMA_LOCAL_H_
#define SRC_EXPERIMENTAL_ASHRUTI_ROMA_GVISOR_INTERFACE_ROMA_LOCAL_H_

#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "src/roma/gvisor/config/config.h"
#include "src/roma/gvisor/container/grpc_client.h"
#include "src/roma/gvisor/interface/roma_interface.h"

namespace privacy_sandbox::server_common::gvisor {
class RomaLocal final : public RomaInterface {
 public:
  // Factory method: creates and returns a RomaLocal.
  // May return null on failure.
  static absl::StatusOr<std::unique_ptr<RomaLocal>> Create(Config config);

  absl::StatusOr<LoadBinaryResponse> LoadBinary(
      std::string_view code_str) override;

  absl::StatusOr<ExecuteBinaryResponse> ExecuteBinary(
      const ExecuteBinaryRequest& request) override;

  ~RomaLocal() override;

 private:
  // Clients can't invoke the constructor directly.
  explicit RomaLocal(Config config, pid_t roma_server_pid,
                     RomaClient roma_client,
                     std::filesystem::path socket_directory)
      : roma_server_pid_(roma_server_pid),
        roma_client_(std::move(roma_client)),
        socket_directory_(std::move(socket_directory)),
        config_(std::move(config)) {}

  const pid_t roma_server_pid_;
  RomaClient roma_client_;
  std::filesystem::path socket_directory_;
  const Config config_;
};
}  // namespace privacy_sandbox::server_common::gvisor

#endif  // SRC_EXPERIMENTAL_ASHRUTI_ROMA_GVISOR_INTERFACE_ROMA_LOCAL_H_
