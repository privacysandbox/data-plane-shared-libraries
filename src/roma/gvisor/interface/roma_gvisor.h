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

#ifndef SRC_ROMA_GVISOR_INTERFACE_ROMA_GVISOR_H_
#define SRC_ROMA_GVISOR_INTERFACE_ROMA_GVISOR_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "src/roma/gvisor/config/config.h"
#include "src/roma/gvisor/interface/roma_interface.h"

namespace privacy_sandbox::server_common::gvisor {
class RomaGvisor final : public RomaInterface {
 public:
  // Factory method: creates and returns a RomaGvisor.
  // May return null on failure.
  static absl::StatusOr<std::unique_ptr<RomaGvisor>> Create(
      Config config, ConfigInternal config_internal,
      std::shared_ptr<::grpc::Channel> channel);

  ~RomaGvisor() override;

 private:
  // Clients can't invoke the constructor directly.
  explicit RomaGvisor(Config config, ConfigInternal config_internal,
                      pid_t roma_container_pid,
                      std::filesystem::path socket_directory)
      : roma_container_pid_(roma_container_pid),
        config_(std::move(config)),
        config_internal_(std::move(config_internal)) {}

  const pid_t roma_container_pid_;
  const Config config_;
  const ConfigInternal config_internal_;
};
}  // namespace privacy_sandbox::server_common::gvisor

#endif  // SRC_ROMA_GVISOR_INTERFACE_ROMA_GVISOR_H_
