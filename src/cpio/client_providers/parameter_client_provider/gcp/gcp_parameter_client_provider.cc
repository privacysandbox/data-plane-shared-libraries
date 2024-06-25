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

#include "gcp_parameter_client_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/strings/substitute.h"
#include "google/cloud/secretmanager/secret_manager_client.h"
#include "google/cloud/secretmanager/secret_manager_connection.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/instance_client_provider/gcp/gcp_instance_client_utils.h"
#include "src/cpio/common/gcp/gcp_utils.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/parameter_service/v1/parameter_service.pb.h"
#include "src/util/status_macro/status_macros.h"

#include "error_codes.h"

using google::cloud::StatusCode;
using google::cloud::secretmanager::MakeSecretManagerServiceConnection;
using google::cloud::secretmanager::SecretManagerServiceClient;
using google::cloud::secretmanager::v1::AccessSecretVersionRequest;
using google::cloud::secretmanager::v1::AccessSecretVersionResponse;
using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncPriority;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_GCP_PARAMETER_CLIENT_PROVIDER_CREATE_SM_CLIENT_FAILURE;
using google::scp::core::errors::
    SC_GCP_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME;
using google::scp::cpio::client_providers::GcpInstanceClientUtils;
using google::scp::cpio::common::GcpUtils;

namespace {
constexpr std::string_view kGcpParameterClientProvider =
    "GcpParameterClientProvider";
constexpr std::string_view kGcpSecretNameFormatString =
    "projects/$0/secrets/$1/versions/latest";
}  // namespace

namespace google::scp::cpio::client_providers {
absl::Status GcpParameterClientProvider::Init() noexcept {
  // Try to get project_id from constructor, otherwise get project_id from
  // running instance_client.
  if (project_id_.empty()) {
    auto project_id_or =
        GcpInstanceClientUtils::GetCurrentProjectId(*instance_client_provider_);
    if (!project_id_or.Successful()) {
      SCP_ERROR(kGcpParameterClientProvider, kZeroUuid, project_id_or.result(),
                "Failed to get project ID for current instance");
      return absl::InternalError(google::scp::core::errors::GetErrorMessage(
          project_id_or.result().status_code));
    }
    project_id_ = std::move(*project_id_or);
  }

  sm_client_shared_ = GetSecretManagerClient();
  if (!sm_client_shared_) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_PARAMETER_CLIENT_PROVIDER_CREATE_SM_CLIENT_FAILURE);
    SCP_ERROR(kGcpParameterClientProvider, kZeroUuid, execution_result,
              "Failed to create secret manager service client.");
    return absl::InternalError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }

  return absl::OkStatus();
}

std::shared_ptr<SecretManagerServiceClient>
GcpParameterClientProvider::GetSecretManagerClient() noexcept {
  return std::make_shared<SecretManagerServiceClient>(
      MakeSecretManagerServiceConnection());
}

absl::Status GcpParameterClientProvider::GetParameter(
    AsyncContext<GetParameterRequest, GetParameterResponse>&
        get_parameter_context) noexcept {
  const auto& secret = get_parameter_context.request->parameter_name();
  if (secret.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME);
    SCP_ERROR_CONTEXT(kGcpParameterClientProvider, get_parameter_context,
                      execution_result, "Failed due to an empty parameter.");
    get_parameter_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }

  std::string name =
      absl::Substitute(kGcpSecretNameFormatString, project_id_, secret);

  AccessSecretVersionRequest access_secret_request;
  access_secret_request.set_name(name);

  auto schedule_result = io_async_executor_->Schedule(
      [this, get_parameter_context,
       access_secret_request = std::move(access_secret_request)]() mutable {
        AsyncGetParameterCallback(get_parameter_context, access_secret_request);
      },
      AsyncPriority::Normal);

  if (!schedule_result.Successful()) {
    get_parameter_context.Finish(schedule_result);

    SCP_ERROR_CONTEXT(kGcpParameterClientProvider, get_parameter_context,
                      schedule_result,
                      "Failed to schedule AsyncGetParameterCallback().");
    return absl::InternalError(google::scp::core::errors::GetErrorMessage(
        schedule_result.status_code));
  }

  return absl::OkStatus();
}

void GcpParameterClientProvider::AsyncGetParameterCallback(
    AsyncContext<GetParameterRequest, GetParameterResponse>&
        get_parameter_context,
    AccessSecretVersionRequest& access_secret_request) noexcept {
  SecretManagerServiceClient client(*sm_client_shared_);

  auto secret_result = client.AccessSecretVersion(access_secret_request);
  auto result = GcpUtils::GcpErrorConverter(secret_result.status());

  if (!result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpParameterClientProvider, get_parameter_context,
                      result, "Failed to get parameter with %s",
                      secret_result.status().message().c_str());

    get_parameter_context.result = result;
    FinishContext(result, get_parameter_context, *async_executor_);
    return;
  }

  auto version = std::move(secret_result).value();
  get_parameter_context.response = std::make_shared<GetParameterResponse>();
  get_parameter_context.response->set_parameter_value(version.payload().data());
  get_parameter_context.result = SuccessExecutionResult();

  FinishContext(SuccessExecutionResult(), get_parameter_context,
                *async_executor_);
}

absl::StatusOr<std::unique_ptr<ParameterClientProviderInterface>>
ParameterClientProviderFactory::Create(
    ParameterClientOptions options,
    absl::Nonnull<InstanceClientProviderInterface*> instance_client_provider,
    absl::Nonnull<core::AsyncExecutorInterface*> cpu_async_executor,
    absl::Nonnull<core::AsyncExecutorInterface*> io_async_executor) {
  auto provider = std::make_unique<GcpParameterClientProvider>(
      cpu_async_executor, io_async_executor, instance_client_provider,
      std::move(options));
  PS_RETURN_IF_ERROR(provider->Init());
  return provider;
}
}  // namespace google::scp::cpio::client_providers
