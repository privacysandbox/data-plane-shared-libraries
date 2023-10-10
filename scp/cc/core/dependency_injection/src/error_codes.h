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

#ifndef CORE_DEPENDENCY_INJECTION_SRC_ERROR_CODES_H_
#define CORE_DEPENDENCY_INJECTION_SRC_ERROR_CODES_H_

#include "public/core/interface/execution_result.h"
#include "scp/cc/core/interface/errors.h"

namespace google::scp::core::errors {

/// Registers component code as 0x0013 for dependency injection.
REGISTER_COMPONENT_CODE(SC_DEPENDENCY_INJECTION, 0x0013)

/// Defines the error code as 0x0001 when the registration has already been
/// registered
DEFINE_ERROR_CODE(SC_DEPENDENCY_INJECTION_COMPONENT_ALREADY_REGISTERED,
                  SC_DEPENDENCY_INJECTION, 0x0001,
                  "The component id has already been registered",
                  HttpStatusCode::CONFLICT)

/// Defines the error code as 0x0001 when the request type is not subscribed.
DEFINE_ERROR_CODE(SC_DEPENDENCY_INJECTION_CYCLE_DETECTED,
                  SC_DEPENDENCY_INJECTION, 0x0002,
                  "Cycle detected in dependency graph",
                  HttpStatusCode::CONFLICT)

/// Defines the error code as 0x0001 when the request type is not subscribed.
DEFINE_ERROR_CODE(SC_DEPENDENCY_INJECTION_UNDEFINED_DEPENDENCY,
                  SC_DEPENDENCY_INJECTION, 0x0003,
                  "A dependency has been declared but not registered",
                  HttpStatusCode::CONFLICT)

/// Defines the error code as 0x0004 when a failure occurs creating the
/// components
DEFINE_ERROR_CODE(SC_DEPENDENCY_INJECTION_ERROR_CREATING_COMPONENTS,
                  SC_DEPENDENCY_INJECTION, 0x0004,
                  "An error occurred while creating the components",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
}  // namespace google::scp::core::errors

#endif  // CORE_DEPENDENCY_INJECTION_SRC_ERROR_CODES_H_