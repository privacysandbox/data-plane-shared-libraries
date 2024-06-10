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

#ifndef CPIO_CLIENT_PROVIDERS_INSTANCE_CLIENT_PROVIDER_MOCK_MOCK_INSTANCE_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_INSTANCE_CLIENT_PROVIDER_MOCK_MOCK_INSTANCE_CLIENT_PROVIDER_H_

#include <gmock/gmock.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"

namespace google::scp::cpio::client_providers::mock {
class MockInstanceClientProvider : public InstanceClientProviderInterface {
 public:
  MOCK_METHOD(
      absl::Status, GetCurrentInstanceResourceName,
      ((core::AsyncContext<cmrt::sdk::instance_service::v1::
                               GetCurrentInstanceResourceNameRequest,
                           cmrt::sdk::instance_service::v1::
                               GetCurrentInstanceResourceNameResponse>&)),
      (override, noexcept));

  MOCK_METHOD(
      absl::Status, GetTagsByResourceName,
      ((core::AsyncContext<
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest,
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse>&)),
      (override, noexcept));

  MOCK_METHOD(
      absl::Status, GetInstanceDetailsByResourceName,
      ((core::AsyncContext<cmrt::sdk::instance_service::v1::
                               GetInstanceDetailsByResourceNameRequest,
                           cmrt::sdk::instance_service::v1::
                               GetInstanceDetailsByResourceNameResponse>&)),
      (override, noexcept));

  MOCK_METHOD(
      absl::Status, ListInstanceDetailsByEnvironment,
      ((core::AsyncContext<cmrt::sdk::instance_service::v1::
                               ListInstanceDetailsByEnvironmentRequest,
                           cmrt::sdk::instance_service::v1::
                               ListInstanceDetailsByEnvironmentResponse>&)),
      (override, noexcept));

  std::string instance_resource_name =
      R"(arn:aws:ec2:us-east-1:123456789012:instance/i-0e9801d129EXAMPLE)";
  absl::Status get_instance_resource_name_mock = absl::OkStatus();

  absl::Status GetCurrentInstanceResourceNameSync(
      std::string& resource_name) noexcept override {
    if (!get_instance_resource_name_mock.ok()) {
      return get_instance_resource_name_mock;
    }
    resource_name = instance_resource_name;
    return absl::OkStatus();
  }

  absl::Status GetInstanceDetailsByResourceNameSync(
      std::string_view resource_name,
      cmrt::sdk::instance_service::v1::InstanceDetails&
          instance_details) noexcept override {
    // Not implemented.
    return absl::UnimplementedError("");
  }
};
}  // namespace google::scp::cpio::client_providers::mock

#endif  // CPIO_CLIENT_PROVIDERS_INSTANCE_CLIENT_PROVIDER_MOCK_MOCK_INSTANCE_CLIENT_PROVIDER_H_
