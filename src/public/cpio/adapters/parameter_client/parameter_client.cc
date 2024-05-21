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

#include "parameter_client.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/functional/bind_front.h"
#include "src/core/common/global_logger/global_logger.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/interface/errors.h"
#include "src/core/utils/error_utils.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/adapters/common/adapter_utils.h"
#include "src/public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetPublicErrorCode;
using google::scp::core::utils::ConvertToPublicExecutionResult;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::ParameterClientProviderFactory;
using google::scp::cpio::client_providers::ParameterClientProviderInterface;

namespace {
constexpr std::string_view kParameterClient = "ParameterClient";
}  // namespace

namespace google::scp::cpio {
ExecutionResult ParameterClient::CreateParameterClientProvider() noexcept {
  cpio_ = &GlobalCpio::GetGlobalCpio();
  InstanceClientProviderInterface* instance_client_provider;
  if (auto provider = cpio_->GetInstanceClientProvider(); !provider.ok()) {
    ExecutionResult execution_result;
    SCP_ERROR(kParameterClient, kZeroUuid, execution_result,
              "Failed to get InstanceClientProvider.");
    return execution_result;
  } else {
    instance_client_provider = *provider;
  }

  AsyncExecutorInterface* cpu_async_executor;
  if (auto executor = cpio_->GetCpuAsyncExecutor(); !executor.ok()) {
    ExecutionResult execution_result;
    SCP_ERROR(kParameterClient, kZeroUuid, execution_result,
              "Failed to get CpuAsyncExecutor.");
    return execution_result;
  } else {
    cpu_async_executor = *executor;
  }

  // TODO(b/321117161): Replace CPU w/ IO executor.
  AsyncExecutorInterface* io_async_executor;
  if (auto executor = cpio_->GetCpuAsyncExecutor(); !executor.ok()) {
    ExecutionResult execution_result;
    SCP_ERROR(kParameterClient, kZeroUuid, execution_result,
              "Failed to get IoAsyncExecutor.");
    return execution_result;
  } else {
    io_async_executor = *executor;
  }

  ParameterClientOptions options = *options_;
  if (options.project_id.empty()) {
    options.project_id = cpio_->GetProjectId();
  }
  if (options.region.empty()) {
    options.region = cpio_->GetRegion();
  }
  parameter_client_provider_ = ParameterClientProviderFactory::Create(
      std::move(options), instance_client_provider, cpu_async_executor,
      io_async_executor);
  return SuccessExecutionResult();
}

ExecutionResult ParameterClient::Init() noexcept {
  auto execution_result = CreateParameterClientProvider();
  if (!execution_result.Successful()) {
    SCP_ERROR(kParameterClient, kZeroUuid, execution_result,
              "Failed to create ParameterClientProvider.");
    return ConvertToPublicExecutionResult(execution_result);
  }

  execution_result = parameter_client_provider_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kParameterClient, kZeroUuid, execution_result,
              "Failed to initialize ParameterClientProvider.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult ParameterClient::Run() noexcept {
  auto execution_result = parameter_client_provider_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kParameterClient, kZeroUuid, execution_result,
              "Failed to run ParameterClientProvider.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult ParameterClient::Stop() noexcept {
  auto execution_result = parameter_client_provider_->Stop();
  if (!execution_result.Successful()) {
    SCP_ERROR(kParameterClient, kZeroUuid, execution_result,
              "Failed to stop ParameterClientProvider.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

core::ExecutionResult ParameterClient::GetParameter(
    GetParameterRequest request,
    Callback<GetParameterResponse> callback) noexcept {
  return Execute<GetParameterRequest, GetParameterResponse>(
      absl::bind_front(&ParameterClientProviderInterface::GetParameter,
                       parameter_client_provider_.get()),
      request, callback);
}

std::unique_ptr<ParameterClientInterface> ParameterClientFactory::Create(
    ParameterClientOptions options) {
  return std::make_unique<ParameterClient>(
      std::make_shared<ParameterClientOptions>(std::move(options)));
}
}  // namespace google::scp::cpio
