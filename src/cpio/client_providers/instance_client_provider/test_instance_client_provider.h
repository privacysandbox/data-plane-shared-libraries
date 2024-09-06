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

#ifndef CPIO_CLIENT_PROVIDERS_INSTANCE_CLIENT_PROVIDER_TEST_INSTANCE_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_INSTANCE_CLIENT_PROVIDER_TEST_INSTANCE_CLIENT_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/core/interface/service_interface.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/test/global_cpio/test_cpio_options.h"

namespace google::scp::cpio::client_providers {
/// Configurations for Test InstanceClientProvider.
struct TestInstanceClientOptions {
  /// Cloud region.
  std::string region;
  /// Instance ID.
  std::string instance_id;
  /// Public IP address.
  std::string public_ipv4_address;
  /// Private IP address.
  std::string private_ipv4_address;
  /// Project ID.
  std::string project_id;
  /// Zone ID.
  std::string zone;
};

/**
 * @copydoc InstanceClientProviderInterface.
 */
class TestInstanceClientProvider : public InstanceClientProviderInterface {
 public:
  explicit TestInstanceClientProvider(TestInstanceClientOptions test_options)
      : test_options_(std::move(test_options)) {}

  absl::Status GetCurrentInstanceResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameResponse>&
          context) noexcept override;

  absl::Status GetTagsByResourceName(
      core::AsyncContext<
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest,
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse>&
          context) noexcept override;

  absl::Status GetInstanceDetailsByResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameResponse>&
          context) noexcept override;

  absl::Status GetCurrentInstanceResourceNameSync(
      std::string& resource_name) noexcept override;

  absl::Status ListInstanceDetailsByEnvironment(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             ListInstanceDetailsByEnvironmentRequest,
                         cmrt::sdk::instance_service::v1::
                             ListInstanceDetailsByEnvironmentResponse>&
          context) noexcept override;

  absl::Status GetInstanceDetailsByResourceNameSync(
      std::string_view resource_name,
      cmrt::sdk::instance_service::v1::InstanceDetails&
          instance_details) noexcept override;

 protected:
  TestInstanceClientOptions test_options_;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_INSTANCE_CLIENT_PROVIDER_TEST_INSTANCE_CLIENT_PROVIDER_H_
