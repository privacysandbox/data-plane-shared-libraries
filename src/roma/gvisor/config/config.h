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

#ifndef SRC_ROMA_GVISOR_CONFIG_CONFIG_H_
#define SRC_ROMA_GVISOR_CONFIG_CONFIG_H_

#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/gvisor/config/utils.h"

namespace privacy_sandbox::server_common::gvisor {
struct Config {
  int num_workers = std::thread::hardware_concurrency();

  std::string roma_container_name = "roma_server";

  std::string lib_mounts = GetLibMounts();

  std::vector<google::scp::roma::FunctionBindingObjectV2<>> function_bindings;
};

struct ConfigInternal {
  // Path to gVisor runsc binary.
  std::filesystem::path runsc_path = GetRunscPath();

  // Absolute path to directory containing OCI container image for Roma Bring
  // Your Own Binary.
  std::filesystem::path roma_container_dir = GetRomaContainerDir();

  std::filesystem::path roma_container_root_dir = GetRomaContainerRootDir();

  std::filesystem::path roma_server_path = GetRomaServerPwd();

  std::string lib_mounts_flag_name = "libs";

  std::string socket_flag_name = "server_socket";

  std::string worker_pool_size_flag_name = "worker_pool_size";

  std::string server_socket;

  std::string callback_socket;
};
}  // namespace privacy_sandbox::server_common::gvisor

#endif  // SRC_ROMA_GVISOR_CONFIG_CONFIG_H_
