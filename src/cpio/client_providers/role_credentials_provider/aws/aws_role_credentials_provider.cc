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

#include "aws_role_credentials_provider.h"

#include <memory>
#include <string>
#include <utility>

#include <aws/sts/model/AssumeRoleRequest.h>

#include "absl/functional/bind_front.h"
#include "src/core/async_executor/aws/aws_async_executor.h"
#include "src/core/common/time_provider/time_provider.h"
#include "src/core/common/uuid/uuid.h"
#include "src/cpio/client_providers/instance_client_provider/aws/aws_instance_client_utils.h"
#include "src/cpio/client_providers/role_credentials_provider/aws/sts_error_converter.h"
#include "src/cpio/common/aws/aws_utils.h"
#include "src/public/core/interface/execution_result.h"
#include "src/util/status_macro/status_macros.h"

#include "error_codes.h"

using Aws::String;
using Aws::Client::AsyncCallerContext;
using Aws::Client::ClientConfiguration;
using Aws::STS::STSClient;
using Aws::STS::Model::AssumeRoleOutcome;
using Aws::STS::Model::AssumeRoleRequest;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AsyncPriority;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::TimeProvider;
using google::scp::core::errors::
    SC_AWS_ROLE_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED;
using google::scp::cpio::client_providers::AwsInstanceClientUtils;

namespace {
constexpr std::string_view kAwsRoleCredentialsProvider =
    "AwsRoleCredentialsProvider";
}

namespace google::scp::cpio::client_providers {
ClientConfiguration AwsRoleCredentialsProvider::CreateClientConfiguration(
    std::string_view region) noexcept {
  return common::CreateClientConfiguration(std::string(region));
}

absl::Status AwsRoleCredentialsProvider::Init() noexcept {
  ClientConfiguration client_config;
  if (!region_code_.empty()) {
    client_config = CreateClientConfiguration(region_code_);
  } else {
    auto region_code_or = AwsInstanceClientUtils::GetCurrentRegionCode(
        *instance_client_provider_);
    if (!region_code_or.Successful()) {
      SCP_ERROR(kAwsRoleCredentialsProvider, kZeroUuid, region_code_or.result(),
                "Failed to get region code for current instance");
      return absl::InternalError(
          core::errors::GetErrorMessage(region_code_or.result().status_code));
    }
    client_config = CreateClientConfiguration(*region_code_or);
  }
  client_config.executor =
      std::make_shared<AwsAsyncExecutor>(io_async_executor_);
  sts_client_ = std::make_shared<STSClient>(std::move(client_config));

  auto timestamp = std::to_string(
      TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks());
  session_name_ = std::make_shared<std::string>(timestamp);

  return absl::OkStatus();
}

absl::Status AwsRoleCredentialsProvider::GetRoleCredentials(
    AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
        get_credentials_context) noexcept {
  AssumeRoleRequest sts_request;
  sts_request.SetRoleArn(*(get_credentials_context.request->account_identity));
  sts_request.SetRoleSessionName(*session_name_);

  sts_client_->AssumeRoleAsync(
      sts_request,
      absl::bind_front(
          &AwsRoleCredentialsProvider::OnGetRoleCredentialsCallback, this,
          get_credentials_context),
      nullptr);

  return absl::OkStatus();
}

void AwsRoleCredentialsProvider::OnGetRoleCredentialsCallback(
    AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
        get_credentials_context,
    const STSClient* sts_client,
    const AssumeRoleRequest& get_credentials_request,
    const AssumeRoleOutcome& get_credentials_outcome,
    const std::shared_ptr<const AsyncCallerContext> async_context) noexcept {
  if (!get_credentials_outcome.IsSuccess()) {
    auto execution_result = STSErrorConverter::ConvertSTSError(
        get_credentials_outcome.GetError().GetErrorType(),
        get_credentials_outcome.GetError().GetMessage());

    // Retries for retriable errors with high priority if specified in the
    // callback of get_credentials_context.
    if (!cpu_async_executor_
             ->Schedule(
                 [get_credentials_context, execution_result]() mutable {
                   get_credentials_context.Finish(execution_result);
                 },
                 AsyncPriority::High)
             .Successful()) {
      get_credentials_context.Finish(execution_result);
    }
    return;
  }

  get_credentials_context.response =
      std::make_shared<GetRoleCredentialsResponse>();
  get_credentials_context.response->access_key_id =
      std::make_shared<std::string>(get_credentials_outcome.GetResult()
                                        .GetCredentials()
                                        .GetAccessKeyId()
                                        .c_str());
  get_credentials_context.response->access_key_secret =
      std::make_shared<std::string>(get_credentials_outcome.GetResult()
                                        .GetCredentials()
                                        .GetSecretAccessKey()
                                        .c_str());
  get_credentials_context.response->security_token =
      std::make_shared<std::string>(get_credentials_outcome.GetResult()
                                        .GetCredentials()
                                        .GetSessionToken()
                                        .c_str());

  get_credentials_context.Finish(SuccessExecutionResult());
}

absl::StatusOr<std::unique_ptr<RoleCredentialsProviderInterface>>
RoleCredentialsProviderFactory::Create(
    RoleCredentialsProviderOptions options,
    absl::Nonnull<InstanceClientProviderInterface*> instance_client_provider,
    absl::Nonnull<core::AsyncExecutorInterface*> cpu_async_executor,
    absl::Nonnull<core::AsyncExecutorInterface*> io_async_executor) noexcept {
  auto provider = std::make_unique<AwsRoleCredentialsProvider>(
      std::move(options), instance_client_provider, cpu_async_executor,
      io_async_executor);
  PS_RETURN_IF_ERROR(provider->Init());
  return provider;
}
}  // namespace google::scp::cpio::client_providers
