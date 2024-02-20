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

#ifndef ROMA_SANDBOX_DISPATCHER_SRC_REQUEST_CONVERTER_H_
#define ROMA_SANDBOX_DISPATCHER_SRC_REQUEST_CONVERTER_H_

#include <string>

#include "scp/cc/roma/interface/roma.h"
#include "scp/cc/roma/sandbox/constants/constants.h"
#include "scp/cc/roma/sandbox/worker_api/src/worker_api.h"

namespace google::scp::roma::sandbox {
namespace internal::request_converter {
/**
 * @brief Converts fields that are common to all request types.
 *
 * @tparam RequestT The request type.
 * @param run_code_request The output request.
 * @param request The input request.
 */
template <typename RequestT>
void RunRequestFromInputRequestCommon(
    worker_api::WorkerApi::RunCodeRequest& run_code_request,
    const RequestT& request) {
  run_code_request
      .metadata[google::scp::roma::sandbox::constants::kCodeVersion] =
      request.version_string;
  run_code_request.metadata[google::scp::roma::sandbox::constants::kRequestId] =
      request.id;

  for (const auto& [key, val] : request.tags) {
    run_code_request.metadata[key] = val;
  }
}

/**
 * @brief Converts fields that are common for invocation requests.
 *
 * @tparam RequestT The type of the invocation request.
 * @param run_code_request The output request.
 * @param request The input request.
 */
template <typename RequestT>
void InvocationRequestCommon(
    worker_api::WorkerApi::RunCodeRequest& run_code_request,
    const RequestT& request, std::string_view request_type) {
  run_code_request
      .metadata[google::scp::roma::sandbox::constants::kRequestAction] =
      google::scp::roma::sandbox::constants::kRequestActionExecute;
  run_code_request
      .metadata[google::scp::roma::sandbox::constants::kHandlerName] =
      request.handler_name;
  run_code_request
      .metadata[google::scp::roma::sandbox::constants::kRequestType] =
      request_type;
  if (request.treat_input_as_byte_str) {
    run_code_request
        .metadata[google::scp::roma::sandbox::constants::kInputType] =
        google::scp::roma::sandbox::constants::kInputTypeBytes;
  }
}
}  // namespace internal::request_converter

struct RequestConverter {
  /**
   * @brief Template specialization for InvocationStrRequest. This converts a
   * InvocationStrRequest into a RunCodeRequest.
   */
  template <typename TMetadata>
  static worker_api::WorkerApi::RunCodeRequest FromUserProvided(
      const InvocationStrRequest<TMetadata>& request,
      std::string_view request_type) {
    worker_api::WorkerApi::RunCodeRequest run_code_request;
    internal::request_converter::RunRequestFromInputRequestCommon(
        run_code_request, request);
    run_code_request.input.reserve(request.input.size());
    for (const auto& i : request.input) {
      run_code_request.input.push_back(i);
    }
    internal::request_converter::InvocationRequestCommon(run_code_request,
                                                         request, request_type);

    return run_code_request;
  }

  /**
   * @brief Template specialization for InvocationSharedRequest. This
   * converts a InvocationSharedRequest into a RunCodeRequest.
   */
  template <typename TMetadata>
  static worker_api::WorkerApi::RunCodeRequest FromUserProvided(
      const InvocationSharedRequest<TMetadata>& request,
      std::string_view request_type) {
    worker_api::WorkerApi::RunCodeRequest run_code_request;
    internal::request_converter::RunRequestFromInputRequestCommon(
        run_code_request, request);
    run_code_request.input.reserve(request.input.size());
    for (const auto& i : request.input) {
      run_code_request.input.push_back(*i);
    }
    internal::request_converter::InvocationRequestCommon(run_code_request,
                                                         request, request_type);

    return run_code_request;
  }

  /**
   * @brief Template specialization for InvocationStrViewRequest. This
   * converts a InvocationStrViewRequest into a RunCodeRequest.
   */
  template <typename TMetadata>
  static worker_api::WorkerApi::RunCodeRequest FromUserProvided(
      const InvocationStrViewRequest<TMetadata>& request,
      std::string_view request_type) {
    worker_api::WorkerApi::RunCodeRequest run_code_request;
    internal::request_converter::RunRequestFromInputRequestCommon(
        run_code_request, request);
    run_code_request.input.reserve(request.input.size());
    for (const auto& i : request.input) {
      run_code_request.input.push_back(i);
    }
    internal::request_converter::InvocationRequestCommon(run_code_request,
                                                         request, request_type);

    return run_code_request;
  }

  /**
   * @brief Template specialization for CodeObject. This converts a CodeObject
   * into a RunCodeRequest.
   */
  static worker_api::WorkerApi::RunCodeRequest FromUserProvided(
      const CodeObject& request, std::string_view request_type) {
    worker_api::WorkerApi::RunCodeRequest run_code_request;
    internal::request_converter::RunRequestFromInputRequestCommon(
        run_code_request, request);
    run_code_request
        .metadata[google::scp::roma::sandbox::constants::kRequestAction] =
        google::scp::roma::sandbox::constants::kRequestActionLoad;
    run_code_request
        .metadata[google::scp::roma::sandbox::constants::kRequestType] =
        request_type;
    run_code_request.code = request.js.empty() ? request.wasm : request.js;
    run_code_request.wasm = request.wasm_bin;
    if (const auto it =
            request.tags.find(google::scp::roma::kWasmCodeArrayName);
        it != request.tags.end()) {
      run_code_request.metadata[google::scp::roma::kWasmCodeArrayName] =
          it->second;
    }
    return run_code_request;
  }
};
}  // namespace google::scp::roma::sandbox

#endif  // ROMA_SANDBOX_DISPATCHER_SRC_REQUEST_CONVERTER_H_
