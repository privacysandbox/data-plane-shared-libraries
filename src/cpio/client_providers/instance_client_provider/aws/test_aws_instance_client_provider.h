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

#ifndef CPIO_CLIENT_PROVIDERS_INSTANCE_CLIENT_PROVIDER_AWS_TEST_AWS_INSTANCE_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_INSTANCE_CLIENT_PROVIDER_AWS_TEST_AWS_INSTANCE_CLIENT_PROVIDER_H_

#include <string>
#include <utility>

#include "src/cpio/client_providers/instance_client_provider/test_instance_client_provider.h"

namespace google::scp::cpio::client_providers {
/**
 * @copydoc TestInstanceClientProvider.
 */
class TestAwsInstanceClientProvider : public TestInstanceClientProvider {
 public:
  explicit TestAwsInstanceClientProvider(TestInstanceClientOptions test_options)
      : TestInstanceClientProvider(std::move(test_options)) {}

  absl::Status GetCurrentInstanceResourceNameSync(
      std::string& resource_name) noexcept override;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_INSTANCE_CLIENT_PROVIDER_AWS_TEST_AWS_INSTANCE_CLIENT_PROVIDER_H_
