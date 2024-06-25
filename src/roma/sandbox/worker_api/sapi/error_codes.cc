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

#include "src/roma/sandbox/worker_api/sapi/error_codes.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "src/util/status_macro/status_builder.h"

namespace {
absl::Status GetAbslStatus(int int_status_code) {
  SapiStatusCode status_code = static_cast<SapiStatusCode>(int_status_code);
  switch (status_code) {
    case SapiStatusCode::kOk:
      return absl::OkStatus();
    case SapiStatusCode::kCouldNotDeserializeInitData:
      return absl::InvalidArgumentError("Failed to deserialize init data");
    case SapiStatusCode::kCouldNotDeserializeRunData:
      return absl::InvalidArgumentError("Failed to deserialize run data");
    case SapiStatusCode::kCouldNotSerializeResponseData:
      return absl::InternalError("Could not serialize response data");
    case SapiStatusCode::kExecutionFailed:
      return absl::InternalError("Execution failed");
    case SapiStatusCode::kFailedToCreateBufferInsideSandboxee:
      return absl::InternalError(
          "Failed to create the Buffer from fd inside the sandboxee");
    case SapiStatusCode::kInvalidDuration:
      return absl::InvalidArgumentError("Failed to convert a duration");
    case SapiStatusCode::kResponseLargerThanBuffer:
      return absl::ResourceExhaustedError(
          "Response size is bigger than buffer");
    case SapiStatusCode::kUninitializedWorker:
      return absl::FailedPreconditionError(
          "A call to run code was issued with an uninitialized worker");
    case SapiStatusCode::kValidSandboxBufferRequired:
      return absl::InternalError(
          "Failed to create a valid sandbox2 buffer for sandbox "
          "communication");

      // No default. This will cause a compile error if a new enum value is
      // added without also updating this switch statement.
  }

  // It's technically valid C++ for an enum to take any int value.  That should
  // never happen, but handle it just in case.
  return absl::UnknownError(
      absl::StrCat("Unexpected value for SapiStatusCode: ", status_code));
}
};  // namespace

absl::Status SapiStatusCodeToAbslStatus(int int_status_code,
                                        std::string_view err_msg) {
  absl::Status status = GetAbslStatus(int_status_code);
  if (err_msg.empty()) {
    return status;
  }
  privacy_sandbox::server_common::StatusBuilder builder(status);
  builder << err_msg;
  return builder;
}
