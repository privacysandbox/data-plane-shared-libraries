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

#ifndef ROMA_SANDBOX_DISPATCHER_REQUEST_CONVERTER_H_
#define ROMA_SANDBOX_DISPATCHER_REQUEST_CONVERTER_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "src/roma/interface/roma.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/worker_api/sapi/worker_params.pb.h"

namespace google::scp::roma::sandbox {
namespace internal::request_converter {
/**
 * @brief Adds fields that are common to all request types.
 */
void AddMetadata(std::string version_string, std::string id,
                 absl::flat_hash_map<std::string, std::string> tags,
                 ::worker_api::WorkerParamsProto& params);
}  // namespace internal::request_converter

/**
 * @brief This converts a CodeObject into a WorkerParamsProto.
 */
::worker_api::WorkerParamsProto RequestToProto(CodeObject request);

/**
 * @brief This converts an InvocationRequest into a WorkerParamsProto.
 */
template <typename InputType, typename TMetadata>
::worker_api::WorkerParamsProto RequestToProto(
    InvocationRequest<InputType, TMetadata> request) {
  ::worker_api::WorkerParamsProto params;
  // Add metadata.
  internal::request_converter::AddMetadata(std::move(request.version_string),
                                           std::move(request.id),
                                           std::move(request.tags), params);
  auto& metadata = *params.mutable_metadata();
  metadata[google::scp::roma::sandbox::constants::kRequestAction] =
      google::scp::roma::sandbox::constants::kRequestActionExecute;
  metadata[google::scp::roma::sandbox::constants::kHandlerName] =
      std::move(request.handler_name);

  // Add inputs.
  if (request.treat_input_as_byte_str) {
    metadata[google::scp::roma::sandbox::constants::kInputType] =
        google::scp::roma::sandbox::constants::kInputTypeBytes;
    if (request.input.size() != 1) {
      return params;
    }
    if constexpr (std::is_same_v<InputType, std::shared_ptr<std::string>>) {
      params.set_input_bytes(*request.input.at(0));
    } else if constexpr (std::is_same_v<InputType, std::string_view>) {
      params.set_input_bytes(std::string(request.input.at(0)));
    } else {
      // InputType is std::string.
      params.set_input_bytes(std::move(request.input.at(0)));
    }
  } else {
    auto& inputs = *params.mutable_input_strings()->mutable_inputs();
    inputs.Reserve(request.input.size());
    for (InputType& input : request.input) {
      if constexpr (std::is_same_v<InputType, std::shared_ptr<std::string>>) {
        *inputs.Add() = *input;
      } else if constexpr (std::is_same_v<InputType, std::string_view>) {
        inputs.Add(std::string(input));
      } else {
        // InputType is std::string.
        inputs.Add(std::move(input));
      }
    }
  }
  return params;
}
}  // namespace google::scp::roma::sandbox

#endif  // ROMA_SANDBOX_DISPATCHER_REQUEST_CONVERTER_H_
