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
#include "scp/cc/core/interface/errors.h"

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
using google::scp::core::HttpClientInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::FailureExecutionResult;

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

ExecutionResult AzureInstanceClientProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AzureInstanceClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AzureInstanceClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AzureInstanceClientProvider::GetCurrentInstanceResourceNameSync(
    std::string& resource_name) noexcept {
  // Not implemented.
  return FailureExecutionResult(SC_UNKNOWN);
}

ExecutionResult AzureInstanceClientProvider::GetCurrentInstanceResourceName(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>&
        get_resource_name_context) noexcept {
  get_resource_name_context.response =
        std::make_shared<GetCurrentInstanceResourceNameResponse>();
  // We need to figure out what we should return here.
  get_resource_name_context.response->set_instance_resource_name(kResourceNameValue);
  get_resource_name_context.result = SuccessExecutionResult();
  get_resource_name_context.Finish();
  return SuccessExecutionResult();
}

ExecutionResult AzureInstanceClientProvider::GetTagsByResourceName(
    AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>&
        get_tags_context) noexcept {
  // Not implemented.
  return FailureExecutionResult(SC_UNKNOWN);
}

ExecutionResult AzureInstanceClientProvider::GetInstanceDetailsByResourceNameSync(
    const std::string& resource_name,
    cmrt::sdk::instance_service::v1::InstanceDetails&
        instance_details) noexcept {
  
  // Not implemented.
  return FailureExecutionResult(SC_UNKNOWN);
}

ExecutionResult AzureInstanceClientProvider::GetInstanceDetailsByResourceName(
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

  *(get_instance_details_context.response->mutable_instance_details()) = instance_details;
  get_instance_details_context.result = SuccessExecutionResult();
  get_instance_details_context.Finish();
  return SuccessExecutionResult();
}

ExecutionResult AzureInstanceClientProvider::ListInstanceDetailsByEnvironment(
    AsyncContext<ListInstanceDetailsByEnvironmentRequest,
                 ListInstanceDetailsByEnvironmentResponse>&
        get_instance_details_context) noexcept {
  // Not implemented.
  return FailureExecutionResult(SC_UNKNOWN);
}

std::shared_ptr<InstanceClientProviderInterface>
InstanceClientProviderFactory::Create(
    const std::shared_ptr<AuthTokenProviderInterface>& auth_token_provider,
    const std::shared_ptr<HttpClientInterface>& http1_client,
    const std::shared_ptr<HttpClientInterface>& http2_client,
    const std::shared_ptr<AsyncExecutorInterface>& async_executor,
    const std::shared_ptr<AsyncExecutorInterface>& io_async_executor) {
  return std::make_shared<AzureInstanceClientProvider>();
}

}  // namespace google::scp::cpio::client_providers
