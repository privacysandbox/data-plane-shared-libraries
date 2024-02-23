
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

#include "src/public/core/interface/execution_result.h"

#include "absl/container/flat_hash_map.h"
#include "src/core/common/proto/common.pb.h"

namespace google::scp::core {
namespace {
absl::flat_hash_map<core::common::proto::ExecutionStatus, ExecutionStatus>
ReverseMap(const absl::flat_hash_map<ExecutionStatus,
                                     core::common::proto::ExecutionStatus>& m) {
  absl::flat_hash_map<core::common::proto::ExecutionStatus, ExecutionStatus> r;
  for (const auto& kv : m) {
    r[kv.second] = kv.first;
  }
  return r;
}

// Static duration maps are heap allocated to avoid destructor call.
const auto& kExecutionStatusToProtoMap =
    *new absl::flat_hash_map<ExecutionStatus,
                             core::common::proto::ExecutionStatus>{
        {ExecutionStatus::Success,
         core::common::proto::ExecutionStatus::EXECUTION_STATUS_SUCCESS},
        {ExecutionStatus::Failure,
         core::common::proto::ExecutionStatus::EXECUTION_STATUS_FAILURE},
        {ExecutionStatus::Retry,
         core::common::proto::ExecutionStatus::EXECUTION_STATUS_RETRY}};

const auto& kProtoToExecutionStatusMap =
    *new absl::flat_hash_map<core::common::proto::ExecutionStatus,
                             ExecutionStatus>(
        ReverseMap(kExecutionStatusToProtoMap));
}  // namespace

core::common::proto::ExecutionStatus ToStatusProto(ExecutionStatus& status) {
  return kExecutionStatusToProtoMap.at(status);
}

core::common::proto::ExecutionResult ExecutionResult::ToProto() {
  core::common::proto::ExecutionResult result_proto;
  result_proto.set_status(ToStatusProto(status));
  result_proto.set_status_code(status_code);
  return result_proto;
}

ExecutionResult::ExecutionResult(
    const core::common::proto::ExecutionResult result_proto) {
  auto mapped_status = ExecutionStatus::Failure;
  // Handle proto status UNKNOWN
  if (const auto it = kProtoToExecutionStatusMap.find(result_proto.status());
      it != kProtoToExecutionStatusMap.end()) {
    mapped_status = it->second;
  }
  status = mapped_status;
  status_code = result_proto.status_code();
}
}  // namespace google::scp::core
