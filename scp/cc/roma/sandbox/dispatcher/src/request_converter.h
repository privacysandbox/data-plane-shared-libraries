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

#include <memory>
#include <string>

#include "roma/interface/roma.h"
#include "roma/sandbox/constants/constants.h"
#include "roma/sandbox/worker_api/src/worker_api.h"

namespace google::scp::roma::sandbox::dispatcher::request_converter {
/**
 * @brief Converts fields that are common to all request types.
 *
 * @tparam RequestT The request type.
 * @param run_code_request The output request.
 * @param request The input request.
 */
template <typename RequestT>
static void RunRequestFromInputRequestCommon(
    worker_api::WorkerApi::RunCodeRequest& run_code_request,
    const RequestT& request) {
  run_code_request
      .metadata[google::scp::roma::sandbox::constants::kCodeVersion] =
      request->version_string;
  run_code_request.metadata[google::scp::roma::sandbox::constants::kRequestId] =
      request->id;

  for (auto& [key, val] : request->tags) {
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
static void InvocationRequestCommon(
    worker_api::WorkerApi::RunCodeRequest& run_code_request,
    const RequestT& request, const std::string& request_type) {
  run_code_request
      .metadata[google::scp::roma::sandbox::constants::kRequestAction] =
      google::scp::roma::sandbox::constants::kRequestActionExecute;
  run_code_request
      .metadata[google::scp::roma::sandbox::constants::kHandlerName] =
      request->handler_name;
  run_code_request
      .metadata[google::scp::roma::sandbox::constants::kRequestType] =
      request_type;
}

template <typename T>
struct RequestConverter {};

/**
 * @brief Template specialization for InvocationStrRequest. This converts a
 * InvocationStrRequest into a RunCodeRequest.
 */
template <typename TMetadata>
struct RequestConverter<InvocationStrRequest<TMetadata>> {
  static core::ExecutionResultOr<worker_api::WorkerApi::RunCodeRequest>
  FromUserProvided(
      const std::unique_ptr<InvocationStrRequest<TMetadata>>& request,
      const std::string& request_type) {
    worker_api::WorkerApi::RunCodeRequest run_code_request;
    RunRequestFromInputRequestCommon<
        std::unique_ptr<InvocationStrRequest<TMetadata>>>(run_code_request,
                                                          request);
    run_code_request.input.reserve(request->input.size());
    for (auto& i : request->input) {
      run_code_request.input.push_back(i);
    }
    InvocationRequestCommon(run_code_request, request, request_type);

    return run_code_request;
  }
};

/**
 * @brief Template specialization for InvocationSharedRequest. This
 * converts a InvocationSharedRequest into a RunCodeRequest.
 */
template <typename TMetadata>
struct RequestConverter<InvocationSharedRequest<TMetadata>> {
  static core::ExecutionResultOr<worker_api::WorkerApi::RunCodeRequest>
  FromUserProvided(
      const std::unique_ptr<InvocationSharedRequest<TMetadata>>& request,
      const std::string& request_type) {
    worker_api::WorkerApi::RunCodeRequest run_code_request;
    RunRequestFromInputRequestCommon<
        std::unique_ptr<InvocationSharedRequest<TMetadata>>>(run_code_request,
                                                             request);
    run_code_request.input.reserve(request->input.size());
    for (auto& i : request->input) {
      run_code_request.input.push_back(*i);
    }
    InvocationRequestCommon(run_code_request, request, request_type);

    return run_code_request;
  }
};

/**
 * @brief Template specialization for InvocationStrViewRequest. This
 * converts a InvocationStrViewRequest into a RunCodeRequest.
 */
template <typename TMetadata>
struct RequestConverter<InvocationStrViewRequest<TMetadata>> {
  static core::ExecutionResultOr<worker_api::WorkerApi::RunCodeRequest>
  FromUserProvided(
      const std::unique_ptr<InvocationStrViewRequest<TMetadata>>& request,
      const std::string& request_type) {
    worker_api::WorkerApi::RunCodeRequest run_code_request;
    RunRequestFromInputRequestCommon<
        std::unique_ptr<InvocationStrViewRequest<TMetadata>>>(run_code_request,
                                                              request);
    run_code_request.input.reserve(request->input.size());
    for (auto& i : request->input) {
      run_code_request.input.push_back(i);
    }
    InvocationRequestCommon(run_code_request, request, request_type);

    return run_code_request;
  }
};

/**
 * @brief Template specialization for CodeObject. This converts a CodeObject
 * into a RunCodeRequest.
 */
template <>
struct RequestConverter<CodeObject> {
  static core::ExecutionResultOr<worker_api::WorkerApi::RunCodeRequest>
  FromUserProvided(const std::unique_ptr<CodeObject>& request,
                   const std::string& request_type) {
    worker_api::WorkerApi::RunCodeRequest run_code_request;
    RunRequestFromInputRequestCommon<std::unique_ptr<CodeObject>>(
        run_code_request, request);
    run_code_request
        .metadata[google::scp::roma::sandbox::constants::kRequestAction] =
        google::scp::roma::sandbox::constants::kRequestActionLoad;
    run_code_request
        .metadata[google::scp::roma::sandbox::constants::kRequestType] =
        request_type;
    run_code_request.code = request->js.empty() ? request->wasm : request->js;
    run_code_request.wasm = request->wasm_bin;
    if (const auto it =
            request->tags.find(google::scp::roma::kWasmCodeArrayName);
        it != request->tags.end()) {
      run_code_request.metadata[google::scp::roma::kWasmCodeArrayName] =
          it->second;
    }
    return run_code_request;
  }
};
}  // namespace google::scp::roma::sandbox::dispatcher::request_converter

#endif  // ROMA_SANDBOX_DISPATCHER_SRC_REQUEST_CONVERTER_H_
