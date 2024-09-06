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

#include "request_validator.h"

#include "absl/status/status.h"
#include "src/roma/interface/roma.h"
#include "src/roma/sandbox/constants/constants.h"

namespace google::scp::roma::sandbox::dispatcher {
/**
 * @brief Template specialization to validate a CodeObject.
 */
absl::Status AssertRequestIsValid(const CodeObject& request) {
  if (request.js.empty() == request.wasm.empty()) {
    return absl::InvalidArgumentError(
        "Exactly one of JS and WASM must be set.");
  }
  if (!request.wasm.empty() && !request.wasm_bin.empty()) {
    return absl::InvalidArgumentError(
        ".wasm and .wasm_bin are mutually exclusive fields.");
  }
  if (!request.wasm_bin.empty() != request.tags.contains(kWasmCodeArrayName)) {
    return absl::InvalidArgumentError(
        "WASM code tag must always and only be set when the WASM code array is "
        "non-empty.");
  }
  return absl::OkStatus();
}
}  // namespace google::scp::roma::sandbox::dispatcher
