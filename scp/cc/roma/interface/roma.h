/*
 * Copyright 2022 Google LLC
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

#ifndef ROMA_INTERFACE_ROMA_H_
#define ROMA_INTERFACE_ROMA_H_

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "roma/config/src/config.h"

namespace google::scp::roma {
enum class [[deprecated(
    "Going forward, this value will be ignored and the only supported return "
    "type will be string.")]] WasmDataType{kUnknownType, kUint32, kString,
                                           kListOfString};
/// @brief The key of timeout tag for request. This tag should be set with a
/// valid absl::Duration string. From Abseil's Time Programming doc, A duration
/// string is a possibly signed sequence of decimal numbers, each with optional
/// fraction and a unit suffix, such as "300ms", "-1.5h" or "2h45m". Valid time
/// units are "ns", "us" "ms", "s", "m", "h".
static constexpr char kTimeoutDurationTag[] = "TimeoutDuration";
/// @brief Default value for request execution timeout. If no timeout tag is
/// set, the default value will be used.
static constexpr absl::Duration kDefaultExecutionTimeout =
    absl::Milliseconds(5000);
/// @brief The wasm code array name tag for request.
static constexpr char kWasmCodeArrayName[] = "roma.request.wasm_array_name";

// The code object containing untrusted code to be loaded into the Worker.
struct CodeObject {
  // The id of the code object.
  std::string id;
  // The version string of the code object.
  std::string version_string;
  // The javascript code to execute. If empty, this code object is wasm only.
  std::string js;
  // The wasm code to be executed in standalone mode.
  std::string wasm;
  // The wasm code array to be loaded and instantiated from the driver JS code.
  std::vector<std::uint8_t> wasm_bin;
  // Any key-value pair tags associated with this code object.
  absl::flat_hash_map<std::string, std::string> tags;
};

/**
 * @brief The invocation request containing handler name and inputs to invoke
 * with the pre-loaded untrusted code. Here, the input is a vector of string or
 * shared pointers to string.
 *
 * @tparam InputType the data type of input vector. Only can be std::string,
 * std::string_view, or std::shared_ptr<std::string>.
 * std::shared_ptr<std::string> is being deprecated.
 */
template <typename InputType, typename TMetadata = DefaultMetadata>
struct InvocationRequest {
  static_assert(
      std::is_same<InputType, std::string>::value ||
          std::is_same<InputType, std::shared_ptr<std::string>>::value ||
          std::is_same<InputType, std::string_view>::value,
      "InputType must be type std::string, std::shared_ptr<std::string>, or "
      "std::string_view");

  // The id of the invocation request.
  std::string id;
  // The version string of the untrusted code that performs the execution
  // object.
  std::string version_string;
  // The signature of the handler function to invoke.
  std::string handler_name;

  // The return type of the WASM handler. For wasm source code execution, this
  // field is required.
  [[deprecated(
      "Going forward, this value will be ignored and the only supported return "
      "type will be string.")]] WasmDataType wasm_return_type;
  // Any key-value pair tags associated with this code object.
  absl::flat_hash_map<std::string, std::string> tags;
  // The input arguments to invoke the handler function. The InputType string is
  // in a format that can be parsed as JSON.
  std::vector<InputType> input;
  // Any server-side metadata associated with this code object. This metadata is
  // passed into native functions without entering SAPI Sandbox and v8.
  TMetadata metadata;
};

template <typename TMetadata = DefaultMetadata>
using InvocationStrRequest = InvocationRequest<std::string, TMetadata>;
template <typename TMetadata = DefaultMetadata>
using InvocationSharedRequest =
    InvocationRequest<std::shared_ptr<std::string>, TMetadata>;
template <typename TMetadata = DefaultMetadata>
using InvocationStrViewRequest = InvocationRequest<std::string_view, TMetadata>;

// The response as result of execution of the code object or invocation request.
struct ResponseObject {
  // The id of the object.
  std::string id;
  // The response of the execution.
  std::string resp;
  // Execution metrics. Any key should be checked for existence.
  absl::flat_hash_map<std::string, absl::Duration> metrics;
};

using Callback =
    absl::AnyInvocable<void(std::unique_ptr<absl::StatusOr<ResponseObject>>)>;

// Batch API
// void Callback(const vector<ResponseObject>&);
using BatchCallback = absl::AnyInvocable<void(
    const std::vector<absl::StatusOr<ResponseObject>>&)>;
}  // namespace google::scp::roma

#endif  // ROMA_INTERFACE_ROMA_H_
