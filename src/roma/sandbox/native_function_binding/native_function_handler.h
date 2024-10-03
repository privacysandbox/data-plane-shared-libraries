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

#ifndef ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_NATIVE_FUNCTION_HANDLER_H_
#define ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_NATIVE_FUNCTION_HANDLER_H_

#ifdef NON_SAPI_BUILD
#include "src/roma/sandbox/native_function_binding/native_function_handler_non_sapi.h"
#else
#include "src/roma/sandbox/native_function_binding/native_function_handler_sapi_ipc.h"
#endif

#endif  // ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_NATIVE_FUNCTION_HANDLER_H_
