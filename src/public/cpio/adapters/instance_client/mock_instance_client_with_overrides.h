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

#ifndef PUBLIC_CPIO_ADAPTERS_INSTANCE_CLIENT_MOCK_INSTANCE_CLIENT_WITH_OVERRIDES_H_
#define PUBLIC_CPIO_ADAPTERS_INSTANCE_CLIENT_MOCK_INSTANCE_CLIENT_WITH_OVERRIDES_H_

#include <memory>

#include "src/cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "src/public/cpio/adapters/instance_client/instance_client.h"

namespace google::scp::cpio::mock {
class MockInstanceClientWithOverrides : public InstanceClient {
 public:
  MockInstanceClientWithOverrides(
      const std::shared_ptr<InstanceClientOptions>& options)
      : InstanceClient(options) {}

  core::ExecutionResult create_instance_client_provider_result =
      core::SuccessExecutionResult();

  core::ExecutionResult CreateInstanceClientProvider() noexcept override {
    if (create_instance_client_provider_result.Successful()) {
      instance_client_provider_ = &mock_instance_client_provider_;
      return create_instance_client_provider_result;
    }
    return create_instance_client_provider_result;
  }

  client_providers::mock::MockInstanceClientProvider&
  GetInstanceClientProvider() {
    return mock_instance_client_provider_;
  }

 private:
  client_providers::mock::MockInstanceClientProvider
      mock_instance_client_provider_;
};
}  // namespace google::scp::cpio::mock

#endif  // PUBLIC_CPIO_ADAPTERS_INSTANCE_CLIENT_MOCK_INSTANCE_CLIENT_WITH_OVERRIDES_H_
