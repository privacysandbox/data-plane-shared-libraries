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

#include "aws_assume_role_credentials_provider.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include <aws/sts/model/AssumeRoleRequest.h>

#include "absl/functional/bind_front.h"
#include "src/core/async_executor/aws/aws_async_executor.h"
#include "src/core/common/time_provider/time_provider.h"
#include "src/public/core/interface/execution_result.h"

#include "error_codes.h"

using Aws::String;
using Aws::Client::AsyncCallerContext;
using Aws::Client::ClientConfiguration;
using Aws::STS::STSClient;
using Aws::STS::Model::AssumeRoleOutcome;
using Aws::STS::Model::AssumeRoleRequest;
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::common::TimeProvider;

namespace {
constexpr std::string_view kAwsAssumeRoleCredentialsProvider =
    "AwsAssumeRoleCredentialsProvider";
}  // namespace

namespace google::scp::core {
ExecutionResult AwsAssumeRoleCredentialsProvider::Init() noexcept {
  if (assume_role_arn_.empty()) {
    return FailureExecutionResult(
        core::errors::SC_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED);
  }

  if (assume_role_external_id_.empty()) {
    return FailureExecutionResult(
        core::errors::SC_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED);
  }

  if (region_.empty()) {
    return FailureExecutionResult(
        core::errors::SC_CREDENTIALS_PROVIDER_INITIALIZATION_FAILED);
  }

  client_config_ = std::make_shared<ClientConfiguration>();
  client_config_->executor =
      std::make_shared<AwsAsyncExecutor>(io_async_executor_);
  client_config_->region = region_;
  sts_client_ = std::make_shared<STSClient>(*client_config_);

  auto timestamp = std::to_string(
      TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks());
  session_name_ = std::make_shared<std::string>(timestamp);
  return SuccessExecutionResult();
};

ExecutionResult AwsAssumeRoleCredentialsProvider::GetCredentials(
    AsyncContext<GetCredentialsRequest, GetCredentialsResponse>&
        get_credentials_context) noexcept {
  AssumeRoleRequest sts_request;

  String assume_role_arn(assume_role_arn_);
  String assume_role_external_id(assume_role_external_id_);
  String session_name(*session_name_);

  sts_request.SetRoleArn(assume_role_arn);
  sts_request.SetExternalId(assume_role_external_id);
  sts_request.SetRoleSessionName(session_name);

  sts_client_->AssumeRoleAsync(
      sts_request,
      absl::bind_front(
          &AwsAssumeRoleCredentialsProvider::OnGetCredentialsCallback, this,
          get_credentials_context),
      nullptr);

  return SuccessExecutionResult();
}

void AwsAssumeRoleCredentialsProvider::OnGetCredentialsCallback(
    AsyncContext<GetCredentialsRequest, GetCredentialsResponse>&
        get_credentials_context,
    const STSClient* sts_client,
    const AssumeRoleRequest& get_credentials_request,
    const AssumeRoleOutcome& get_credentials_outcome,
    const std::shared_ptr<const AsyncCallerContext> async_context) noexcept {
  if (!get_credentials_outcome.IsSuccess()) {
    SCP_DEBUG_CONTEXT(
        kAwsAssumeRoleCredentialsProvider, get_credentials_context,
        "AwsAssumeRoleCredentialsProvider assume role request failed. "
        "Error code: %d, message: %s",
        get_credentials_outcome.GetError().GetResponseCode(),
        get_credentials_outcome.GetError().GetMessage().c_str());

    auto execution_result = FailureExecutionResult(
        errors::SC_CREDENTIALS_PROVIDER_FAILED_TO_FETCH_CREDENTIALS);
    if (!async_executor_
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

  get_credentials_context.response = std::make_shared<GetCredentialsResponse>();
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

}  // namespace google::scp::core
