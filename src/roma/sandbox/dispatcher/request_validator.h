/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ROMA_SANDBOX_DISPATCHER_REQUEST_VALIDATOR_H_
#define ROMA_SANDBOX_DISPATCHER_REQUEST_VALIDATOR_H_

#include <memory>

#include "absl/status/status.h"
#include "src/roma/interface/roma.h"

namespace google::scp::roma::sandbox::dispatcher {
/**
 * @brief Template specialization to validate a CodeObject.
 */
absl::Status AssertRequestIsValid(const CodeObject& request);

/**
 * @brief Common validation fields for invocation requests.
 */
template <typename InputType, typename TMetadata>
absl::Status AssertRequestIsValid(
    const InvocationRequest<InputType, TMetadata>& request) {
  if (request.treat_input_as_byte_str && request.input.size() != 1) {
    return absl::InvalidArgumentError(
        "Dispatch is disallowed since the number of inputs does not equal one "
        "and InvocationRequest.treat_input_as_byte_str is true.");
  }
  return absl::OkStatus();
}
}  // namespace google::scp::roma::sandbox::dispatcher

#endif  // ROMA_SANDBOX_DISPATCHER_REQUEST_VALIDATOR_H_
