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

#include "lib_cpio_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/any.pb.h"
#include "src/core/async_executor/async_executor.h"
#include "src/core/common/global_logger/global_logger.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/curl_client/http1_curl_client.h"
#include "src/core/http2_client/http2_client.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/interface/errors.h"
#include "src/core/interface/http_client_interface.h"
#include "src/core/interface/message_router_interface.h"
#include "src/core/interface/service_interface.h"
#include "src/core/message_router/message_router.h"
#include "src/cpio/client_providers/interface/auth_token_provider_interface.h"
#include "src/cpio/client_providers/interface/cloud_initializer_interface.h"
#include "src/cpio/client_providers/interface/cpio_provider_interface.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "src/public/core/interface/execution_result.h"

using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::Http1CurlClient;
using google::scp::core::HttpClient;
using google::scp::core::HttpClientInterface;
using google::scp::core::MessageRouter;
using google::scp::core::MessageRouterInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetErrorMessage;

namespace {
constexpr std::string_view kLibCpioProvider = "LibCpioProvider";
constexpr size_t kThreadPoolThreadCount = 2;
constexpr size_t kThreadPoolQueueSize = 100000;
constexpr size_t kIOThreadPoolThreadCount = 2;
constexpr size_t kIOThreadPoolQueueSize = 100000;
}  // namespace

namespace google::scp::cpio::client_providers {
ExecutionResult LibCpioProvider::Init() noexcept {
  if (cpio_options_.cloud_init_option == CloudInitOption::kInitInCpio) {
    cloud_initializer_ = CloudInitializerFactory::Create();
    auto execution_result = cloud_initializer_->Init();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to init cloud initializer.");
      return execution_result;
    }
  }
  return SuccessExecutionResult();
}

ExecutionResult LibCpioProvider::Run() noexcept {
  if (cpio_options_.cloud_init_option == CloudInitOption::kInitInCpio) {
    auto execution_result = cloud_initializer_->Run();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to run cloud initializer.");
      return execution_result;
    }
    cloud_initializer_->InitCloud();
  }
  return SuccessExecutionResult();
}

ExecutionResult LibCpioProvider::Stop() noexcept {
  if (instance_client_provider_) {
    auto execution_result = instance_client_provider_->Stop();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to stop instance client provider.");
      return execution_result;
    }
  }

  if (auth_token_provider_) {
    auto execution_result = auth_token_provider_->Stop();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to stop auth token provider.");
      return execution_result;
    }
  }

  if (http2_client_) {
    auto execution_result = http2_client_->Stop();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to stop http2 client.");
      return execution_result;
    }
  }

  if (http1_client_) {
    auto execution_result = http1_client_->Stop();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to stop http1 client.");
      return execution_result;
    }
  }

  if (cpu_async_executor_) {
    auto execution_result = cpu_async_executor_->Stop();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to stop CPU async executor.");
      return execution_result;
    }
  }

  if (io_async_executor_) {
    auto execution_result = io_async_executor_->Stop();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to stop IO async executor.");
      return execution_result;
    }
  }

  if (cpio_options_.cloud_init_option == CloudInitOption::kInitInCpio) {
    cloud_initializer_->ShutdownCloud();
    auto execution_result = cloud_initializer_->Stop();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to stop cloud initializer.");
      return execution_result;
    }
  }

  return SuccessExecutionResult();
}

absl::StatusOr<HttpClientInterface*> LibCpioProvider::GetHttpClient() noexcept {
  if (http2_client_) {
    return http2_client_.get();
  }

  auto cpu_async_executor = GetCpuAsyncExecutor();
  if (!cpu_async_executor.ok()) {
    return cpu_async_executor.status();
  }

  auto http2_client = std::make_unique<HttpClient>(*cpu_async_executor);
  if (const auto execution_result = http2_client->Init();
      !execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to initialize http2 client.");
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to initialize http2 client:\n",
                     GetErrorMessage(execution_result.status_code)));
  }

  if (const auto execution_result = http2_client->Run();
      !execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to run http2 client.");
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to run http2 client:\n",
                     GetErrorMessage(execution_result.status_code)));
  }
  http2_client_ = std::move(http2_client);
  return http2_client_.get();
}

absl::StatusOr<HttpClientInterface*>
LibCpioProvider::GetHttp1Client() noexcept {
  if (http1_client_) {
    return http1_client_.get();
  }

  auto cpu_async_executor = GetCpuAsyncExecutor();
  if (!cpu_async_executor.ok()) {
    return cpu_async_executor.status();
  }

  auto io_async_executor = GetIoAsyncExecutor();
  if (!io_async_executor.ok()) {
    return io_async_executor.status();
  }

  auto http1_client = std::make_unique<Http1CurlClient>(*cpu_async_executor,
                                                        *io_async_executor);
  if (const auto execution_result = http1_client->Init();
      !execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to initialize http1 client.");
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to initialize http1 client:\n",
                     GetErrorMessage(execution_result.status_code)));
  }

  if (const auto execution_result = http1_client->Run();
      !execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to run http1 client.");
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to run http1 client:\n",
                     GetErrorMessage(execution_result.status_code)));
  }
  http1_client_ = std::move(http1_client);
  return http1_client_.get();
}

absl::StatusOr<AsyncExecutorInterface*>
LibCpioProvider::GetCpuAsyncExecutor() noexcept {
  if (cpu_async_executor_) {
    return cpu_async_executor_.get();
  }

  auto cpu_async_executor = std::make_unique<AsyncExecutor>(
      kThreadPoolThreadCount, kThreadPoolQueueSize);
  if (const auto execution_result = cpu_async_executor->Init();
      !execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to initialize async executor.");
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to initialize async executor:\n",
                     GetErrorMessage(execution_result.status_code)));
  }

  if (const auto execution_result = cpu_async_executor->Run();
      !execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to run async executor.");
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to run async executor:\n",
                     GetErrorMessage(execution_result.status_code)));
  }
  cpu_async_executor_ = std::move(cpu_async_executor);
  return cpu_async_executor_.get();
}

absl::StatusOr<AsyncExecutorInterface*>
LibCpioProvider::GetIoAsyncExecutor() noexcept {
  if (io_async_executor_) {
    return io_async_executor_.get();
  }

  auto io_async_executor = std::make_unique<AsyncExecutor>(
      kIOThreadPoolThreadCount, kIOThreadPoolQueueSize);
  if (const auto execution_result = io_async_executor->Init();
      !execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to initialize IO async executor.");
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to initialize IO async executor:\n",
                     GetErrorMessage(execution_result.status_code)));
  }

  if (const auto execution_result = io_async_executor->Run();
      !execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to run IO async executor.");
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to run IO async executor:\n",
                     GetErrorMessage(execution_result.status_code)));
  }
  io_async_executor_ = std::move(io_async_executor);
  return io_async_executor_.get();
}

absl::StatusOr<InstanceClientProviderInterface*>
LibCpioProvider::GetInstanceClientProvider() noexcept {
  if (instance_client_provider_) {
    return instance_client_provider_.get();
  }

  auto auth_token_provider = GetAuthTokenProvider();
  if (!auth_token_provider.ok()) {
    return auth_token_provider.status();
  }

  auto http1_client = GetHttp1Client();
  if (!http1_client.ok()) {
    return http1_client.status();
  }

  auto http2_client = GetHttpClient();
  if (!http2_client.ok()) {
    return http2_client.status();
  }

  auto cpu_async_executor = GetCpuAsyncExecutor();
  if (!cpu_async_executor.ok()) {
    return cpu_async_executor.status();
  }

  auto io_async_executor = GetIoAsyncExecutor();
  if (!io_async_executor.ok()) {
    return io_async_executor.status();
  }

  auto instance_client_provider = InstanceClientProviderFactory::Create(
      *auth_token_provider, *http1_client, *http2_client, *cpu_async_executor,
      *io_async_executor);
  if (const auto execution_result = instance_client_provider->Init();
      !execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to initialize instance client provider.");
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to initialize instance client provider:\n",
                     GetErrorMessage(execution_result.status_code)));
  }

  if (const auto execution_result = instance_client_provider->Run();
      !execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to run instance client provider.");
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to run instance client provider:\n",
                     GetErrorMessage(execution_result.status_code)));
  }
  instance_client_provider_ = std::move(instance_client_provider);
  return instance_client_provider_.get();
}

std::unique_ptr<RoleCredentialsProviderInterface>
LibCpioProvider::CreateRoleCredentialsProvider(
    InstanceClientProviderInterface* instance_client_provider,
    AsyncExecutorInterface* cpu_async_executor,
    AsyncExecutorInterface* io_async_executor) noexcept {
  return RoleCredentialsProviderFactory::Create(
      RoleCredentialsProviderOptions(), instance_client_provider,
      cpu_async_executor, io_async_executor);
}

absl::StatusOr<RoleCredentialsProviderInterface*>
LibCpioProvider::GetRoleCredentialsProvider() noexcept {
  if (role_credentials_provider_) {
    return role_credentials_provider_.get();
  }

  auto cpu_async_executor = GetCpuAsyncExecutor();
  if (!cpu_async_executor.ok()) {
    return cpu_async_executor.status();
  }

  auto io_async_executor = GetIoAsyncExecutor();
  if (!io_async_executor.ok()) {
    return io_async_executor.status();
  }

  auto instance_client = GetInstanceClientProvider();
  if (!instance_client.ok()) {
    return instance_client.status();
  }

  auto role_credentials_provider = CreateRoleCredentialsProvider(
      *instance_client, *cpu_async_executor, *io_async_executor);
  if (const auto execution_result = role_credentials_provider->Init();
      !execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to initialize role credential provider.");
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to initialize role credential provider:\n",
                     GetErrorMessage(execution_result.status_code)));
  }

  if (const auto execution_result = role_credentials_provider->Run();
      !execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to run role credential provider.");
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to run role credential provider:\n",
                     GetErrorMessage(execution_result.status_code)));
  }
  role_credentials_provider_ = std::move(role_credentials_provider);
  return role_credentials_provider_.get();
}

absl::StatusOr<AuthTokenProviderInterface*>
LibCpioProvider::GetAuthTokenProvider() noexcept {
  if (auth_token_provider_) {
    return auth_token_provider_.get();
  }

  auto http1_client = GetHttp1Client();
  if (!http1_client.ok()) {
    return http1_client.status();
  }

  auto auth_token_provider = AuthTokenProviderFactory::Create(*http1_client);
  if (const auto execution_result = auth_token_provider->Init();
      !execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to initialize auth token provider.");
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to initialize auth token provider:\n",
                     GetErrorMessage(execution_result.status_code)));
  }

  if (const auto execution_result = auth_token_provider->Run();
      !execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to run role  auth token provider.");
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to run role  auth token provider:\n",
                     GetErrorMessage(execution_result.status_code)));
  }
  auth_token_provider_ = std::move(auth_token_provider);
  return auth_token_provider_.get();
}

const std::string& LibCpioProvider::GetProjectId() noexcept {
  return cpio_options_.project_id;
}

const std::string& LibCpioProvider::GetRegion() noexcept {
  return cpio_options_.region;
}

std::unique_ptr<CpioProviderInterface> CpioProviderFactory::Create(
    CpioOptions options) {
  return std::make_unique<LibCpioProvider>(std::move(options));
}
}  // namespace google::scp::cpio::client_providers
