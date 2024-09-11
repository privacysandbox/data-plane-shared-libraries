// Portions Copyright (c) Microsoft Corporation
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

#ifndef CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_AZURE_ERROR_CODES_H_
#define CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_AZURE_ERROR_CODES_H_

#include "src/core/interface/errors.h"
#include "src/public/cpio/interface/error_codes.h"

namespace google::scp::core::errors {
REGISTER_COMPONENT_CODE(SC_AZURE_KMS_CLIENT_PROVIDER, 0x022C)

DEFINE_ERROR_CODE(SC_AZURE_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND,
                  SC_AZURE_KMS_CLIENT_PROVIDER, 0x0001,
                  "Cannot find cipher text",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_AZURE_KMS_CLIENT_PROVIDER_KEY_ID_NOT_FOUND,
                  SC_AZURE_KMS_CLIENT_PROVIDER, 0x0002,
                  "Cannot find decryption Key ID",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_AZURE_KMS_CLIENT_PROVIDER_BAD_UNWRAPPED_KEY,
                  SC_AZURE_KMS_CLIENT_PROVIDER, 0x0003,
                  "Unwrapped key is malformed.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_AZURE_KMS_CLIENT_PROVIDER_WRAPPING_KEY_GENERATION_ERROR,
                  SC_AZURE_KMS_CLIENT_PROVIDER, 0x0004,
                  "Error during generation of wrapping key.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_AZURE_KMS_CLIENT_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND,
                  SC_AZURE_KMS_CLIENT_PROVIDER, 0x0005,
                  "Credential provider is not found.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_AZURE_KMS_CLIENT_PROVIDER_KEY_HASH_CREATION_ERROR,
                  SC_AZURE_KMS_CLIENT_PROVIDER, 0x0006,
                  "Error generating key hash.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_AZURE_KMS_CLIENT_PROVIDER_EVP_TO_PEM_CONVERSION_ERROR,
                  SC_AZURE_KMS_CLIENT_PROVIDER, 0x0007,
                  "Error converting EVP key to PEM.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_AZURE_KMS_CLIENT_PROVIDER_UNWRAPPING_DECRYPTED_KEY_ERROR,
                  SC_AZURE_KMS_CLIENT_PROVIDER, 0x0008,
                  "Error unwrapping decrypted key.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(SC_AZURE_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND,
                         SC_CPIO_INTERNAL_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(SC_AZURE_KMS_CLIENT_PROVIDER_KEY_ID_NOT_FOUND,
                         SC_CPIO_INTERNAL_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(SC_AZURE_KMS_CLIENT_PROVIDER_BAD_UNWRAPPED_KEY,
                         SC_CPIO_INTERNAL_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(
    SC_AZURE_KMS_CLIENT_PROVIDER_WRAPPING_KEY_GENERATION_ERROR,
    SC_CPIO_INTERNAL_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(
    SC_AZURE_KMS_CLIENT_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND,
    SC_CPIO_INTERNAL_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(SC_AZURE_KMS_CLIENT_PROVIDER_KEY_HASH_CREATION_ERROR,
                         SC_CPIO_INTERNAL_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(
    SC_AZURE_KMS_CLIENT_PROVIDER_EVP_TO_PEM_CONVERSION_ERROR,
    SC_CPIO_INTERNAL_ERROR)

MAP_TO_PUBLIC_ERROR_CODE(
    SC_AZURE_KMS_CLIENT_PROVIDER_UNWRAPPING_DECRYPTED_KEY_ERROR,
    SC_CPIO_INTERNAL_ERROR)
}  // namespace google::scp::core::errors

#endif  // CPIO_CLIENT_PROVIDERS_KMS_CLIENT_PROVIDER_AZURE_ERROR_CODES_H_
