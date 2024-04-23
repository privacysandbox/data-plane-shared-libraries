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

#include "aws_parameter_client_provider.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <aws/core/utils/Outcome.h>
#include <aws/ssm/SSMClient.h>
#include <aws/ssm/model/GetParameterRequest.h>

#include "absl/functional/bind_front.h"
#include "src/core/async_executor/aws/aws_async_executor.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/cpio/client_providers/instance_client_provider/aws/aws_instance_client_utils.h"
#include "src/cpio/common/aws/aws_utils.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

#include "error_codes.h"
#include "ssm_error_converter.h"

using Aws::Client::AsyncCallerContext;
using Aws::Client::ClientConfiguration;
using Aws::SSM::SSMClient;
using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_AWS_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME;
using google::scp::cpio::client_providers::AwsInstanceClientUtils;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::common::CreateClientConfiguration;

namespace {
/// Filename for logging errors
constexpr std::string_view kAwsParameterClientProvider =
    "AwsParameterClientProvider";
}  // namespace

namespace google::scp::cpio::client_providers {
ClientConfiguration AwsParameterClientProvider::CreateClientConfiguration(
    std::string_view region) noexcept {
  return common::CreateClientConfiguration(region);
}

ExecutionResult AwsParameterClientProvider::Init() noexcept {
  // Try to get region code from Global Cpio Options, otherwise get region code
  // from running instance_client.
  if (!region_code_.empty()) {
    ssm_client_ = ssm_client_factory_->CreateSSMClient(
        CreateClientConfiguration(region_code_), io_async_executor_);
  } else {
    auto region_code_or = AwsInstanceClientUtils::GetCurrentRegionCode(
        *instance_client_provider_);
    if (!region_code_or.Successful()) {
      SCP_ERROR(kAwsParameterClientProvider, kZeroUuid, region_code_or.result(),
                "Failed to get region code for current instance");
      return region_code_or.result();
    }
    ssm_client_ = ssm_client_factory_->CreateSSMClient(
        CreateClientConfiguration(*region_code_or), io_async_executor_);
  }

  return SuccessExecutionResult();
}

ExecutionResult AwsParameterClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsParameterClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult AwsParameterClientProvider::GetParameter(
    AsyncContext<GetParameterRequest, GetParameterResponse>&
        get_parameter_context) noexcept {
  if (get_parameter_context.request->parameter_name().empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME);
    SCP_ERROR_CONTEXT(kAwsParameterClientProvider, get_parameter_context,
                      execution_result,
                      "Failed to get the parameter value for %s.",
                      get_parameter_context.request->parameter_name().c_str());
    get_parameter_context.Finish(execution_result);
    return execution_result;
  }

  Aws::SSM::Model::GetParameterRequest request;
  request.SetName(get_parameter_context.request->parameter_name().c_str());

  ssm_client_->GetParameterAsync(
      request,
      absl::bind_front(&AwsParameterClientProvider::OnGetParameterCallback,
                       this, get_parameter_context),
      nullptr);

  return SuccessExecutionResult();
}

void AwsParameterClientProvider::OnGetParameterCallback(
    AsyncContext<GetParameterRequest, GetParameterResponse>&
        get_parameter_context,
    const Aws::SSM::SSMClient*, const Aws::SSM::Model::GetParameterRequest&,
    const Aws::SSM::Model::GetParameterOutcome& outcome,
    const std::shared_ptr<const AsyncCallerContext>&) noexcept {
  if (!outcome.IsSuccess()) {
    auto error_type = outcome.GetError().GetErrorType();
    get_parameter_context.Finish(SSMErrorConverter::ConvertSSMError(
        error_type, outcome.GetError().GetMessage()));
    return;
  }
  get_parameter_context.response = std::make_shared<GetParameterResponse>();
  get_parameter_context.response->set_parameter_value(
      outcome.GetResult().GetParameter().GetValue().c_str());
  get_parameter_context.Finish(SuccessExecutionResult());
}

std::unique_ptr<SSMClient> SSMClientFactory::CreateSSMClient(
    ClientConfiguration client_config,
    AsyncExecutorInterface* io_async_executor) noexcept {
  client_config.executor =
      std::make_shared<AwsAsyncExecutor>(io_async_executor);
  return std::make_unique<SSMClient>(std::move(client_config));
}

std::unique_ptr<ParameterClientProviderInterface>
ParameterClientProviderFactory::Create(
    ParameterClientOptions options,
    InstanceClientProviderInterface* instance_client_provider,
    core::AsyncExecutorInterface* /*cpu_async_executor*/,
    core::AsyncExecutorInterface* io_async_executor) {
  return std::make_unique<AwsParameterClientProvider>(
      std::move(options), instance_client_provider, io_async_executor);
}
}  // namespace google::scp::cpio::client_providers
