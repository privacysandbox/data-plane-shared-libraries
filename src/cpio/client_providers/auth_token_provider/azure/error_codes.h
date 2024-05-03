/*
 * Portions Copyright (c) Microsoft Corporation
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

#ifndef CPIO_CLIENT_PROVIDERS_AUTH_TOKEN_PROVIDER_AZURE_ERROR_CODES_H_
#define CPIO_CLIENT_PROVIDERS_AUTH_TOKEN_PROVIDER_AZURE_ERROR_CODES_H_

#include "src/core/interface/errors.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/error_codes.h"

namespace google::scp::core::errors {

REGISTER_COMPONENT_CODE(SC_AZURE_INSTANCE_AUTHORIZER_PROVIDER, 0x020E)

DEFINE_ERROR_CODE(SC_AZURE_INSTANCE_AUTHORIZER_PROVIDER_INITIALIZATION_FAILED,
                  SC_AZURE_INSTANCE_AUTHORIZER_PROVIDER, 0x0001,
                  "Cannot initialize the GCP instance authorizer provider.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_AZURE_INSTANCE_AUTHORIZER_PROVIDER_BAD_SESSION_TOKEN,
                  SC_AZURE_INSTANCE_AUTHORIZER_PROVIDER, 0x0002,
                  "Session token is malformed.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(
    SC_AZURE_INSTANCE_AUTHORIZER_PROVIDER_INITIALIZATION_FAILED,
    SC_CPIO_COMPONENT_FAILED_INITIALIZED)
MAP_TO_PUBLIC_ERROR_CODE(
    SC_AZURE_INSTANCE_AUTHORIZER_PROVIDER_BAD_SESSION_TOKEN,
    SC_CPIO_INVALID_RESOURCE)
}  // namespace google::scp::core::errors

#endif  // CPIO_CLIENT_PROVIDERS_AUTH_TOKEN_PROVIDER_AZURE_ERROR_CODES_H_
