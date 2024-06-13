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

#ifndef SRC_ROMA_GVISOR_INTERFACE_ROMA_INTERFACE_H_
#define SRC_ROMA_GVISOR_INTERFACE_ROMA_INTERFACE_H_

#include <string>

#include "absl/status/statusor.h"
#include "src/roma/gvisor/interface/roma_api.pb.h"

namespace privacy_sandbox::server_common::gvisor {

// Roma Interface (abstract) class

class RomaInterface {
 public:
  virtual absl::StatusOr<std::string> LoadBinary(
      std::string_view code_path) = 0;

  virtual absl::StatusOr<ExecuteBinaryResponse> ExecuteBinary(
      const ExecuteBinaryRequest& request) = 0;

  virtual ~RomaInterface() = default;
};
}  // namespace privacy_sandbox::server_common::gvisor

#endif  // SRC_ROMA_GVISOR_INTERFACE_ROMA_INTERFACE_H_
