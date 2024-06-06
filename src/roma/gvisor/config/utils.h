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

#ifndef SRC_ROMA_GVISOR_CONFIG_UTILS_H_
#define SRC_ROMA_GVISOR_CONFIG_UTILS_H_

#include <filesystem>
#include <memory>
#include <string>

#include <grpcpp/channel.h>

#include "absl/status/statusor.h"

namespace privacy_sandbox::server_common::gvisor {

std::filesystem::path GetRomaContainerDir();

std::filesystem::path GetRomaContainerRootDir();

std::filesystem::path GetRomaServerPwd();

std::filesystem::path GetRunscPath();

std::string GetLibMounts();

absl::StatusOr<std::filesystem::path> CreateUniqueSocketName();

absl::Status HealthCheckWithExponentialBackoff(
    const std::shared_ptr<grpc::Channel>& channel);

}  // namespace privacy_sandbox::server_common::gvisor

#endif  // SRC_ROMA_GVISOR_CONFIG_UTILS_H_
