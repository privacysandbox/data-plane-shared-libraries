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

#ifndef CPIO_CLIENT_PROVIDERS_GLOBAL_CPIO_MOCK_MOCK_LIB_CPIO_PROVIDER_WITH_OVERRIDES_H_
#define CPIO_CLIENT_PROVIDERS_GLOBAL_CPIO_MOCK_MOCK_LIB_CPIO_PROVIDER_WITH_OVERRIDES_H_

#include <functional>
#include <memory>

#include "absl/status/statusor.h"
#include "src/cpio/client_providers/global_cpio/cpio_provider/lib_cpio_provider.h"
#include "src/cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"

namespace google::scp::cpio::client_providers::mock {
class MockLibCpioProviderWithOverrides : public LibCpioProvider {
 public:
  MockLibCpioProviderWithOverrides() : LibCpioProvider(CpioOptions()) {}

  absl::StatusOr<InstanceClientProviderInterface*>
  GetInstanceClientProvider() noexcept override {
    return &instance_client_provider_;
  }

 private:
  MockInstanceClientProvider instance_client_provider_;
};
}  // namespace google::scp::cpio::client_providers::mock

#endif  // CPIO_CLIENT_PROVIDERS_GLOBAL_CPIO_MOCK_MOCK_LIB_CPIO_PROVIDER_WITH_OVERRIDES_H_
