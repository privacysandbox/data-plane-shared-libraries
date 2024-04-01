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

#include <memory>
#include <utility>

#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
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
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;

namespace google::scp::cpio {
absl::Status InstanceClient::Init() noexcept { return absl::OkStatus(); }

absl::Status InstanceClient::Run() noexcept { return absl::OkStatus(); }

absl::Status InstanceClient::Stop() noexcept { return absl::OkStatus(); }

absl::Status InstanceClient::GetCurrentInstanceResourceName(
    GetCurrentInstanceResourceNameRequest request,
    Callback<GetCurrentInstanceResourceNameResponse> callback) noexcept {
  return Execute<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>(
      absl::bind_front(
          &InstanceClientProviderInterface::GetCurrentInstanceResourceName,
          instance_client_provider_),
      request, callback);
}

absl::Status InstanceClient::GetTagsByResourceName(
    GetTagsByResourceNameRequest request,
    Callback<GetTagsByResourceNameResponse> callback) noexcept {
  return Execute<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>(
      absl::bind_front(&InstanceClientProviderInterface::GetTagsByResourceName,
                       instance_client_provider_),
      request, callback);
}

absl::Status InstanceClient::GetInstanceDetailsByResourceName(
    GetInstanceDetailsByResourceNameRequest request,
    Callback<GetInstanceDetailsByResourceNameResponse> callback) noexcept {
  return Execute<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>(
      absl::bind_front(
          &InstanceClientProviderInterface::GetInstanceDetailsByResourceName,
          instance_client_provider_),
      request, callback);
}

absl::Status InstanceClient::ListInstanceDetailsByEnvironment(
    ListInstanceDetailsByEnvironmentRequest request,
    Callback<ListInstanceDetailsByEnvironmentResponse> callback) noexcept {
  return Execute<ListInstanceDetailsByEnvironmentRequest,
                 ListInstanceDetailsByEnvironmentResponse>(
      absl::bind_front(
          &InstanceClientProviderInterface::ListInstanceDetailsByEnvironment,
          instance_client_provider_),
      request, callback);
}

std::unique_ptr<InstanceClientInterface> InstanceClientFactory::Create() {
  return std::make_unique<InstanceClient>(
      &GlobalCpio::GetGlobalCpio().GetInstanceClientProvider());
}
}  // namespace google::scp::cpio
