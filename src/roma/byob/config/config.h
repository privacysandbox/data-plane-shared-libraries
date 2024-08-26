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

#include <string>
#include <vector>

#include "src/roma/config/function_binding_object_v2.h"

namespace privacy_sandbox::server_common::byob {
template <typename TMetadata = ::google::scp::roma::DefaultMetadata>
struct Config {
  int num_workers;
  std::string roma_container_name;
  std::string lib_mounts;
  std::vector<google::scp::roma::FunctionBindingObjectV2<TMetadata>>
      function_bindings;
};
}  // namespace privacy_sandbox::server_common::byob

#endif  // SRC_ROMA_GVISOR_CONFIG_CONFIG_H_
