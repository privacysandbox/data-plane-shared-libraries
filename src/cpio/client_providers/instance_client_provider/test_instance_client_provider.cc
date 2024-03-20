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

#include "test_instance_client_provider.h"

#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/substitute.h"
#include "src/public/core/interface/execution_result.h"

using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::
    ListInstanceDetailsByEnvironmentRequest;
using google::cmrt::sdk::instance_service::v1::
    ListInstanceDetailsByEnvironmentResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;

namespace {
constexpr std::string_view kAwsResourceNameFormat =
    R"(arn:aws:ec2:$0:$1:instance/$2)";
}  // namespace

namespace google::scp::cpio::client_providers {
absl::Status TestInstanceClientProvider::GetInstanceDetailsByResourceNameSync(
    std::string_view resource_name,
    cmrt::sdk::instance_service::v1::InstanceDetails&
        instance_details) noexcept {
  instance_details.set_instance_id(test_options_.instance_id);
  auto* network = instance_details.add_networks();
  network->set_private_ipv4_address(test_options_.private_ipv4_address);
  network->set_public_ipv4_address(test_options_.public_ipv4_address);
  return absl::OkStatus();
}

absl::Status TestInstanceClientProvider::GetCurrentInstanceResourceName(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>& context) noexcept {
  return absl::OkStatus();
}

absl::Status TestInstanceClientProvider::GetTagsByResourceName(
    AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>&
        context) noexcept {
  // Not implemented.
  return absl::OkStatus();
}

absl::Status TestInstanceClientProvider::GetInstanceDetailsByResourceName(
    AsyncContext<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>& context) noexcept {
  // Not implemented.
  return absl::OkStatus();
}

absl::Status TestInstanceClientProvider::ListInstanceDetailsByEnvironment(
    AsyncContext<ListInstanceDetailsByEnvironmentRequest,
                 ListInstanceDetailsByEnvironmentResponse>& context) noexcept {
  // Not implemented.
  return absl::OkStatus();
}

absl::Status TestInstanceClientProvider::GetCurrentInstanceResourceNameSync(
    std::string& resource_name) noexcept {
  resource_name =
      absl::Substitute(kAwsResourceNameFormat, test_options_.region,
                       test_options_.project_id, test_options_.instance_id);
  return absl::OkStatus();
}

}  // namespace google::scp::cpio::client_providers
