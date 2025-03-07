// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "instance_client_provider.h"

#include <nlohmann/json.hpp>

#include "absl/base/nullability.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/interface/http_client_interface.h"
#include "src/cpio/client_providers/interface/auth_token_provider_interface.h"

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
using json = nlohmann::json;

namespace google::scp::cpio::client_providers {
namespace {

std::string GetEnvVarOrDefault(
    absl::string_view name,
    absl::string_view default_value = kEnvVarValueMissing) {
  if (const char* value = getenv(name.data()); value != nullptr) {
    return std::string(value);
  }

  return std::string(default_value);
}

}  // namespace

KubernetesInstanceClientProvider::KubernetesInstanceClientProvider() {}

absl::Status KubernetesInstanceClientProvider::GetCurrentInstanceResourceName(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>&
        get_resource_name_context) noexcept {
  get_resource_name_context.response =
      std::make_shared<GetCurrentInstanceResourceNameResponse>();

  get_resource_name_context.response->set_instance_resource_name(
      GetEnvVarOrDefault(kPodResourceIdEnvVar));
  get_resource_name_context.Finish(SuccessExecutionResult());
  return absl::OkStatus();
}

absl::Status KubernetesInstanceClientProvider::GetInstanceDetailsByResourceName(
    AsyncContext<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>&
        get_instance_details_context) noexcept {
  get_instance_details_context.response =
      std::make_shared<GetInstanceDetailsByResourceNameResponse>();
  InstanceDetails pod_details;
  pod_details.set_instance_id(
      get_instance_details_context.request->instance_resource_name());

  auto& pod_labels = *pod_details.mutable_labels();

  json label_json;
  try {
    label_json = json::parse(GetEnvVarOrDefault(kPodLabelsEnvVar, ""));
  } catch (const json::parse_error& e) {
    return absl::InvalidArgumentError(e.what());
  }

  for (auto& [key, value] : label_json.items()) {
    pod_labels[key] = value;
  }

  *(get_instance_details_context.response->mutable_instance_details()) =
      pod_details;
  get_instance_details_context.Finish(SuccessExecutionResult());
  return absl::OkStatus();
}

// The rest of the interface is left as unimplemented because it is not used.

absl::Status
KubernetesInstanceClientProvider::GetCurrentInstanceResourceNameSync(
    std::string& resource_name) noexcept {
  // Not implemented.
  return absl::UnimplementedError("GetCurrentInstanceResourceNameSync");
}

absl::Status KubernetesInstanceClientProvider::GetTagsByResourceName(
    AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>&
        get_tags_context) noexcept {
  // Not implemented.
  return absl::UnimplementedError("GetTagsByResourceName");
}

absl::Status
KubernetesInstanceClientProvider::GetInstanceDetailsByResourceNameSync(
    std::string_view resource_name,
    cmrt::sdk::instance_service::v1::InstanceDetails&
        instance_details) noexcept {
  // Not implemented.
  return absl::UnimplementedError("GetInstanceDetailsByResourceNameSync");
}

absl::Status KubernetesInstanceClientProvider::ListInstanceDetailsByEnvironment(
    AsyncContext<ListInstanceDetailsByEnvironmentRequest,
                 ListInstanceDetailsByEnvironmentResponse>&
        get_instance_details_context) noexcept {
  // Not implemented.
  return absl::UnimplementedError("ListInstanceDetailsByEnvironment");
}

absl::Nonnull<std::unique_ptr<InstanceClientProviderInterface>>
InstanceClientProviderFactory::Create(
    absl::Nonnull<AuthTokenProviderInterface*> /*auth_token_provider*/,
    absl::Nonnull<HttpClientInterface*> /*http1_client*/,
    absl::Nonnull<HttpClientInterface*> /*http2_client*/,
    absl::Nonnull<AsyncExecutorInterface*> /*async_executor*/,
    absl::Nonnull<AsyncExecutorInterface*> /*io_async_executor*/) {
  return std::make_unique<KubernetesInstanceClientProvider>();
}

}  // namespace google::scp::cpio::client_providers
