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

#ifndef CPIO_CLIENT_PROVIDERS_CLOUD_INITIALIZER_AZURE_NO_OP_INITIALIZER_H_
#define CPIO_CLIENT_PROVIDERS_CLOUD_INITIALIZER_AZURE_NO_OP_INITIALIZER_H_

#include "src/cpio/client_providers/interface/cloud_initializer_interface.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc CloudInitializerInterface
 */
class NoOpInitializer : public CloudInitializerInterface {
 public:
  void InitCloud() noexcept override;

  void ShutdownCloud() noexcept override;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_CLOUD_INITIALIZER_AZURE_NO_OP_INITIALIZER_H_
