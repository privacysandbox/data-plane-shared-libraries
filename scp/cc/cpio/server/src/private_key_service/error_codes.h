// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {

/// Registers component code as 0x020D for private key service.
REGISTER_COMPONENT_CODE(SC_PRIVATE_KEY_SERVICE, 0x020D);

DEFINE_ERROR_CODE(
    SC_PRIVATE_KEY_SERVICE_MISMATCHED_SECONDARY_ENDPOINTS,
    SC_PRIVATE_KEY_SERVICE, 0x0001,
    "Mismatched secondary endpoints, regions or account identities.",
    HttpStatusCode::BAD_REQUEST);
}  // namespace google::scp::core::errors
