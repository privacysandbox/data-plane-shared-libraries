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

#include "azure_instance_client_provider.h"

#include "absl/log/check.h"
#include "src/core/interface/errors.h"

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
using google::cmrt::sdk::instance_service::v1::InstanceDetails;
using google::cmrt::sdk::instance_service::v1::
    ListInstanceDetailsByEnvironmentRequest;
using google::cmrt::sdk::instance_service::v1::
    ListInstanceDetailsByEnvironmentResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::SuccessExecutionResult;

namespace google::scp::cpio::client_providers {

inline constexpr char kOperatorTagName[] = "operator";
inline constexpr char kEnvironmentTagName[] = "environment";
inline constexpr char kServiceTagName[] = "service";
// Dummy values
inline constexpr char kResourceNameValue[] = "azure_instance_resource_name";
inline constexpr char kInstanceId[] = "azure_instance_id";
inline constexpr char kOperatorTagValue[] = "azure_operator";
inline constexpr char kEnvironmentTagValue[] = "azure_environment";
inline constexpr char kServiceTagValue[] = "azure_service";

AzureInstanceClientProvider::AzureInstanceClientProvider() {}

absl::Status AzureInstanceClientProvider::GetCurrentInstanceResourceNameSync(
    std::string& resource_name) noexcept {
  // Not implemented.
  return absl::UnimplementedError("");
}

absl::Status AzureInstanceClientProvider::GetCurrentInstanceResourceName(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>&
        get_resource_name_context) noexcept {
  get_resource_name_context.response =
      std::make_shared<GetCurrentInstanceResourceNameResponse>();
  // We need to figure out what we should return here.
  get_resource_name_context.response->set_instance_resource_name(
      kResourceNameValue);
  get_resource_name_context.result = SuccessExecutionResult();
  get_resource_name_context.Finish();
  return absl::OkStatus();
}

absl::Status AzureInstanceClientProvider::GetTagsByResourceName(
    AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>&
        get_tags_context) noexcept {
  // Not implemented.
  return absl::UnimplementedError("");
}

absl::Status AzureInstanceClientProvider::GetInstanceDetailsByResourceNameSync(
    std::string_view resource_name,
    cmrt::sdk::instance_service::v1::InstanceDetails&
        instance_details) noexcept {
  // Not implemented.
  return absl::UnimplementedError("");
}

absl::Status AzureInstanceClientProvider::GetInstanceDetailsByResourceName(
    AsyncContext<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>&
        get_instance_details_context) noexcept {
  get_instance_details_context.response =
      std::make_shared<GetInstanceDetailsByResourceNameResponse>();
  // We need to igure out what we should return here.
  InstanceDetails instance_details;
  instance_details.set_instance_id(kInstanceId);

  // We need to provide network info here.
  // auto* network = instance_details.add_networks();
  // network->set_private_ipv4_address(std::move(private_ip));
  // network->set_public_ipv4_address(std::move(public_ip));

  auto& labels_proto = *instance_details.mutable_labels();
  labels_proto[kOperatorTagName] = kOperatorTagValue;
  labels_proto[kEnvironmentTagName] = kEnvironmentTagValue;
  labels_proto[kServiceTagName] = kServiceTagValue;

  *(get_instance_details_context.response->mutable_instance_details()) =
      instance_details;
  get_instance_details_context.result = SuccessExecutionResult();
  get_instance_details_context.Finish();
  return absl::OkStatus();
}

absl::Status AzureInstanceClientProvider::ListInstanceDetailsByEnvironment(
    AsyncContext<ListInstanceDetailsByEnvironmentRequest,
                 ListInstanceDetailsByEnvironmentResponse>&
        get_instance_details_context) noexcept {
  // Not implemented.
  return absl::UnimplementedError("");
}

std::unique_ptr<InstanceClientProviderInterface>
InstanceClientProviderFactory::Create(
    AuthTokenProviderInterface* auth_token_provider,
    HttpClientInterface* http1_client, HttpClientInterface* http2_client,
    AsyncExecutorInterface* async_executor,
    AsyncExecutorInterface* io_async_executor) {
  std::cout << "Returning Azure Instance Client Provider";
  return std::make_unique<AzureInstanceClientProvider>();
}

}  // namespace google::scp::cpio::client_providers
