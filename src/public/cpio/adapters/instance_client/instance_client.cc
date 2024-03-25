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

#include "instance_client.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/functional/bind_front.h"
#include "src/core/common/global_logger/global_logger.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/interface/errors.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/adapters/common/adapter_utils.h"
#include "src/public/cpio/proto/instance_service/v1/instance_service.pb.h"

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
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetPublicErrorCode;
using google::scp::core::utils::ConvertToPublicExecutionResult;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;

namespace google::scp::cpio {
void InstanceClient::CreateInstanceClientProvider() noexcept {
  cpio_ = &GlobalCpio::GetGlobalCpio();
  instance_client_provider_ = &cpio_->GetInstanceClientProvider();
}

ExecutionResult InstanceClient::Init() noexcept {
  CreateInstanceClientProvider();
  return SuccessExecutionResult();
}

ExecutionResult InstanceClient::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult InstanceClient::Stop() noexcept {
  return SuccessExecutionResult();
}

core::ExecutionResult InstanceClient::GetCurrentInstanceResourceName(
    GetCurrentInstanceResourceNameRequest request,
    Callback<GetCurrentInstanceResourceNameResponse> callback) noexcept {
  return Execute<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>(
             absl::bind_front(&InstanceClientProviderInterface::
                                  GetCurrentInstanceResourceName,
                              instance_client_provider_),
             request, callback)
                 .ok()
             ? core::SuccessExecutionResult()
             : core::FailureExecutionResult(SC_UNKNOWN);
}

core::ExecutionResult InstanceClient::GetTagsByResourceName(
    GetTagsByResourceNameRequest request,
    Callback<GetTagsByResourceNameResponse> callback) noexcept {
  return Execute<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>(
             absl::bind_front(
                 &InstanceClientProviderInterface::GetTagsByResourceName,
                 instance_client_provider_),
             request, callback)
                 .ok()
             ? core::SuccessExecutionResult()
             : core::FailureExecutionResult(SC_UNKNOWN);
}

core::ExecutionResult InstanceClient::GetInstanceDetailsByResourceName(
    GetInstanceDetailsByResourceNameRequest request,
    Callback<GetInstanceDetailsByResourceNameResponse> callback) noexcept {
  return Execute<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>(
             absl::bind_front(&InstanceClientProviderInterface::
                                  GetInstanceDetailsByResourceName,
                              instance_client_provider_),
             request, callback)
                 .ok()
             ? core::SuccessExecutionResult()
             : core::FailureExecutionResult(SC_UNKNOWN);
}

core::ExecutionResult InstanceClient::ListInstanceDetailsByEnvironment(
    ListInstanceDetailsByEnvironmentRequest request,
    Callback<ListInstanceDetailsByEnvironmentResponse> callback) noexcept {
  return Execute<ListInstanceDetailsByEnvironmentRequest,
                 ListInstanceDetailsByEnvironmentResponse>(
             absl::bind_front(&InstanceClientProviderInterface::
                                  ListInstanceDetailsByEnvironment,
                              instance_client_provider_),
             request, callback)
                 .ok()
             ? core::SuccessExecutionResult()
             : core::FailureExecutionResult(SC_UNKNOWN);
}

std::unique_ptr<InstanceClientInterface> InstanceClientFactory::Create(
    InstanceClientOptions options) {
  return std::make_unique<InstanceClient>(std::move(options));
}
}  // namespace google::scp::cpio
