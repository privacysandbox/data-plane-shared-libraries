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

#include "request_converter.h"

#include <memory>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "src/roma/interface/roma.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/worker_api/sapi/worker_params.pb.h"

namespace google::scp::roma::sandbox {
namespace internal::request_converter {
void AddMetadata(std::string version_string, std::string id,
                 absl::flat_hash_map<std::string, std::string> tags,
                 ::worker_api::WorkerParamsProto& params) {
  // TODO(b/324272432): Change metadata to a repeated field and reserve here.
  auto& metadata = *params.mutable_metadata();
  metadata[google::scp::roma::sandbox::constants::kCodeVersion] =
      std::move(version_string);
  metadata[google::scp::roma::sandbox::constants::kRequestId] = std::move(id);
  for (auto& [key, val] : tags) {
    metadata[std::move(key)] = std::move(val);
  }
}
}  // namespace internal::request_converter

::worker_api::WorkerParamsProto RequestToProto(CodeObject request) {
  ::worker_api::WorkerParamsProto params;
  // Converting from uint8 array to string necessitates a copy.
  params.set_wasm(
      std::string(request.wasm_bin.begin(), request.wasm_bin.end()));
  internal::request_converter::AddMetadata(std::move(request.version_string),
                                           std::move(request.id),
                                           std::move(request.tags), params);
  auto& metadata = *params.mutable_metadata();
  metadata[google::scp::roma::sandbox::constants::kRequestAction] =
      google::scp::roma::sandbox::constants::kRequestActionLoad;
  auto get_request_type = [](const auto& request) {
    if (!request.wasm_bin.empty()) {
      return constants::kRequestTypeJavascriptWithWasm;
    } else if (request.js.empty()) {
      return constants::kRequestTypeWasm;
    } else {
      return constants::kRequestTypeJavascript;
    }
  };
  metadata[constants::kRequestType] = get_request_type(request);
  if (!request.js.empty()) {
    params.set_code(std::move(request.js));
  } else {
    params.set_code(std::move(request.wasm));
  }
  return params;
}
}  // namespace google::scp::roma::sandbox
