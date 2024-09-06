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

#ifndef CPIO_CLIENT_PROVIDERS_GLOBAL_CPIO_CPIO_PROVIDER_TEST_LIB_CPIO_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_GLOBAL_CPIO_CPIO_PROVIDER_TEST_LIB_CPIO_PROVIDER_H_

#include <memory>

#include "src/core/interface/async_executor_interface.h"
#include "src/cpio/client_providers/global_cpio/cpio_provider/lib_cpio_provider.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "src/public/cpio/test/global_cpio/test_cpio_options.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc LibCpioProvider
 */
class TestLibCpioProvider final : public LibCpioProvider {
 public:
  explicit TestLibCpioProvider(TestCpioOptions test_options);
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_GLOBAL_CPIO_CPIO_PROVIDER_TEST_LIB_CPIO_PROVIDER_H_
