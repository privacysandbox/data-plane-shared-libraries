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

#include <string>
#include <utility>

#include "absl/status/status.h"
#include "src/roma/byob/sample_udf/sample_callback.pb.h"

namespace privacy_sandbox::server_common::byob {
template <typename TMetadata>
std::pair<SampleCallbackResponse, absl::Status> HandleSampleCallback(
    const TMetadata& metadata, const SampleCallbackRequest& request) {
  SampleCallbackResponse response;
  return std::make_pair(std::move(response), absl::OkStatus());
}

template <typename TMetadata>
std::pair<CallbackReadResponse, absl::Status> HandleCallbackRead(
    const TMetadata& metadata, const CallbackReadRequest& request) {
  int64_t payload_size = 0;
  for (const auto& p : request.payloads()) {
    payload_size += p.size();
  }
  CallbackReadResponse response;
  response.set_payload_size(payload_size);
  return std::make_pair(std::move(response), absl::OkStatus());
}

template <typename TMetadata>
std::pair<CallbackWriteResponse, absl::Status> HandleCallbackWrite(
    const TMetadata& metadata, const CallbackWriteRequest& request) {
  CallbackWriteResponse response;
  auto* payloads = response.mutable_payloads();
  payloads->Reserve(request.element_count());
  for (auto i = 0; i < request.element_count(); ++i) {
    payloads->Add(std::string(request.element_size(), 'a'));
  }
  return std::make_pair(std::move(response), absl::OkStatus());
}
}  // namespace privacy_sandbox::server_common::byob
