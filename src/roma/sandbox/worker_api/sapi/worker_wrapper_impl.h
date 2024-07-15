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

// This is a wrapper to be run within the sandboxed API (sapi).
// This wrapper is used to interact with the worker from outside the sandbox,
// but these are the actual functions that the sandbox infra calls.
// The extern "C" is to avoid name mangling to enable sapi code generation.

#ifndef ROMA_SANDBOX_WORKER_API_SAPI_WORKER_WRAPPER_H_
#define ROMA_SANDBOX_WORKER_API_SAPI_WORKER_WRAPPER_H_

#include <string>
#include <vector>

#include "sandboxed_api/lenval_core.h"
#include "src/roma/sandbox/worker_api/sapi/error_codes.h"

// All of the types used for these functions, that're wrapped by SAPI, must be
// C types and cannot be complex C++ types.

extern "C" SapiStatusCode InitFromSerializedData(sapi::LenValStruct* data);

extern "C" SapiStatusCode Run();

extern "C" SapiStatusCode Stop();

/// @brief The sandbox API, which is used to execute all requests, has two data
/// sharing mechanisms:
/// <b>Sharing by sandbox2::Buffer:</b> This mechanism uses a pre-allocated
/// buffer of a fixed size. If the data to be shared is larger than the buffer,
/// the second mechanism is used.
/// <b>Sharing by SAPI variable sapi::LenValStruct<b>: This mechanism allows
/// sharing data of any size.
/// @param data The request and response data are shared with the SAPI variable
/// sapi::LenValStruct when they are passed into or out of the sandboxee.
/// @param input_serialized_size The input variable serialized_size is used to
/// deserialize the request protobuf from the Buffer data inside the sandboxee.
/// @param output_serialized_size The output variable serialized_size is used to
/// deserialize the response protobuf from the Buffer data in the host binary.
/// @return
extern "C" SapiStatusCode RunCodeFromSerializedData(
    sapi::LenValStruct* data, int input_serialized_size,
    size_t* output_serialized_size);

/// @brief The sandbox API, in which the data shared with the Buffer only.
/// @param input_serialized_size The input variable serialized_size is used to
/// deserialize the request protobuf from the Buffer data inside the sandboxee.
/// @param output_serialized_size The output variable serialized_size is used to
/// deserialize the response protobuf from the Buffer data in the host binary.
/// @return
extern "C" SapiStatusCode RunCodeFromBuffer(int input_serialized_size,
                                            size_t* output_serialized_size);

#endif  // ROMA_SANDBOX_WORKER_API_SAPI_WORKER_WRAPPER_H_
