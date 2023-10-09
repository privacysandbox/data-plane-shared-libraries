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

#pragma once

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {
REGISTER_COMPONENT_CODE(SC_ROMA_WORKER_API, 0x0B00)
DEFINE_ERROR_CODE(SC_ROMA_WORKER_API_UNINITIALIZED_WORKER, SC_ROMA_WORKER_API,
                  0x0001,
                  "A call to run code was issued with an uninitialized worker.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_WORKER_API_COULD_NOT_INITIALIZE_SANDBOX,
                  SC_ROMA_WORKER_API, 0x0002,
                  "Could not initialize the SAPI sandbox.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_ROMA_WORKER_API_COULD_NOT_CREATE_IPC_PROTO, SC_ROMA_WORKER_API, 0x0003,
    "Could not create the IPC proto to send information to sandbox.",
    HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_ROMA_WORKER_API_COULD_NOT_GET_PROTO_MESSAGE_AFTER_EXECUTION,
    SC_ROMA_WORKER_API, 0x0004,
    "Could not get the IPC proto message after sandbox execution.",
    HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_WORKER_API_COULD_NOT_INITIALIZE_WRAPPER_API,
                  SC_ROMA_WORKER_API, 0x0005,
                  "Could not initialize the wrapper API.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_WORKER_API_COULD_NOT_RUN_WRAPPER_API,
                  SC_ROMA_WORKER_API, 0x0006, "Could not run the wrapper API.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_WORKER_API_COULD_NOT_RUN_CODE_THROUGH_WRAPPER_API,
                  SC_ROMA_WORKER_API, 0x0007,
                  "Could not run code through the wrapper API.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_ROMA_WORKER_API_COULD_NOT_TRANSFER_FUNCTION_FD_TO_SANDBOXEE,
    SC_ROMA_WORKER_API, 0x0008,
    "Could not transfer function comms fd to sandboxee.",
    HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_WORKER_API_UNINITIALIZED_SANDBOX, SC_ROMA_WORKER_API,
                  0x0009,
                  "Attempt to call API function with an uninitialized sandbox.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_WORKER_API_WORKER_CRASHED, SC_ROMA_WORKER_API, 0x000A,
                  "Sandbox worker crashed during execution of request.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_WORKER_API_COULD_NOT_SERIALIZE_INIT_DATA,
                  SC_ROMA_WORKER_API, 0x000B, "Failed to serialize init data.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_WORKER_API_COULD_NOT_DESERIALIZE_INIT_DATA,
                  SC_ROMA_WORKER_API, 0x000C,
                  "Failed to deserialize init data.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_WORKER_API_COULD_NOT_SERIALIZE_RUN_CODE_DATA,
                  SC_ROMA_WORKER_API, 0x000D,
                  "Failed to serialize run_code data.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_WORKER_API_COULD_NOT_DESERIALIZE_RUN_CODE_DATA,
                  SC_ROMA_WORKER_API, 0x000E,
                  "Failed to deserialize run_code data.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ROMA_WORKER_API_COULD_NOT_SERIALIZE_RUN_CODE_RESPONSE_DATA,
                  SC_ROMA_WORKER_API, 0x000F,
                  "Failed to serialize run_code response data.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_ROMA_WORKER_API_VALID_SANDBOX_BUFFER_REQUIRED, SC_ROMA_WORKER_API,
    0x0010,
    "Failed to create a valid sandbox2 buffer for sandbox communication.",
    HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(
    SC_ROMA_WORKER_API_REQUEST_DATA_SIZE_LARGER_THAN_BUFFER_CAPACITY,
    SC_ROMA_WORKER_API, 0x0011,
    "The size of request serialized data is larger than the Buffer capacity.",
    HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(
    SC_ROMA_WORKER_API_RESPONSE_DATA_SIZE_LARGER_THAN_BUFFER_CAPACITY,
    SC_ROMA_WORKER_API, 0x0012,
    "The size of response serialized data is larger than the Buffer capacity.",
    HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_ROMA_WORKER_API_FAILED_CREATE_BUFFER_INSIDE_SANDBOXEE,
                  SC_ROMA_WORKER_API, 0x0013,
                  "Failed to create the Buffer from fd inside the sandboxee.",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_ROMA_WORKER_API_COULD_NOT_TRANSFER_BUFFER_FD_TO_SANDBOXEE,
                  SC_ROMA_WORKER_API, 0x0014,
                  "Could not transfer sandbox2::Buffer fd to sandboxee.",
                  HttpStatusCode::BAD_REQUEST)

}  // namespace google::scp::core::errors
