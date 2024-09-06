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

#ifndef ROMA_SANDBOX_WORKER_API_SAPI_ERROR_CODES_H_
#define ROMA_SANDBOX_WORKER_API_SAPI_ERROR_CODES_H_

#include <string_view>

#include "absl/status/status.h"

// It would be preferable to pass an absl::Status in and out of SAPI, but that's
// a C++ type and SAPI only allows simple C types.
//
// We could pass an absl::StatusCode, as that's an enum/int, but that would
// lose the details about exactly what caused the error as there are relatively
// few types of canonical ABSL errors.
//
// So we have a custom enum with all of the detailed error types and a
// conversion back to absl::Status.
enum class SapiStatusCode : int {
  kOk = 0,

  kCouldNotDeserializeInitData = 1,
  kCouldNotDeserializeRunData = 2,
  kCouldNotSerializeResponseData = 3,
  kExecutionFailed = 4,
  kFailedToCreateBufferInsideSandboxee = 5,
  kInvalidDuration = 6,
  kResponseLargerThanBuffer = 7,
  kUninitializedWorker = 8,
  kValidSandboxBufferRequired = 9,
};

// Convert the status code enum value into an absl::Status, with a message.
// SAPI changes the namespace that the enum uses and so it's easier to convert
// it to an int and pass that around instead.
absl::Status SapiStatusCodeToAbslStatus(int int_status_code,
                                        std::string_view err_msg = "");

#endif  // ROMA_SANDBOX_WORKER_API_SAPI_ERROR_CODES_H_
