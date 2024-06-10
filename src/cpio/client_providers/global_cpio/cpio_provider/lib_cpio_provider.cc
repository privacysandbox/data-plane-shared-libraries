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
LibCpioProvider::LibCpioProvider(CpioOptions options)
    : project_id_(std::move(options.project_id)),
      region_(std::move(options.region)) {
  if (options.cloud_init_option == CloudInitOption::kInitInCpio) {
    cloud_initializer_ = CloudInitializerFactory::Create();
    cloud_initializer_->InitCloud();
  }
}

ExecutionResult LibCpioProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult LibCpioProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult LibCpioProvider::Stop() noexcept {
  if (http2_client_) {
    auto execution_result = http2_client_->Stop();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to stop http2 client.");
      return execution_result;
    }
  }

  if (cloud_initializer_) {
    cloud_initializer_->ShutdownCloud();
  }

  return SuccessExecutionResult();
}

HttpClientInterface& LibCpioProvider::GetHttpClient() noexcept {
  if (!http2_client_) {
    http2_client_ = std::make_unique<HttpClient>(&GetCpuAsyncExecutor());
  }
  return *http2_client_;
}

HttpClientInterface& LibCpioProvider::GetHttp1Client() noexcept {
  if (!http1_client_) {
    http1_client_ = std::make_unique<Http1CurlClient>(&GetCpuAsyncExecutor(),
                                                      &GetIoAsyncExecutor());
  }
  return *http1_client_;
}

AsyncExecutorInterface& LibCpioProvider::GetCpuAsyncExecutor() noexcept {
  if (!cpu_async_executor_) {
    cpu_async_executor_ = std::make_unique<AsyncExecutor>(
        kThreadPoolThreadCount, kThreadPoolQueueSize);
  }
  return *cpu_async_executor_;
}

AsyncExecutorInterface& LibCpioProvider::GetIoAsyncExecutor() noexcept {
  if (!io_async_executor_) {
    io_async_executor_ = std::make_unique<AsyncExecutor>(
        kIOThreadPoolThreadCount, kIOThreadPoolQueueSize);
  }
  return *io_async_executor_;
}

InstanceClientProviderInterface&
LibCpioProvider::GetInstanceClientProvider() noexcept {
  if (!instance_client_provider_) {
    instance_client_provider_ = InstanceClientProviderFactory::Create(
        &GetAuthTokenProvider(), &GetHttp1Client(), &GetHttpClient(),
        &GetCpuAsyncExecutor(), &GetIoAsyncExecutor());
  }
  return *instance_client_provider_;
}

absl::StatusOr<std::unique_ptr<RoleCredentialsProviderInterface>>
LibCpioProvider::CreateRoleCredentialsProvider(
    RoleCredentialsProviderOptions options,
    InstanceClientProviderInterface* instance_client_provider,
    AsyncExecutorInterface* cpu_async_executor,
    AsyncExecutorInterface* io_async_executor) noexcept {
  return RoleCredentialsProviderFactory::Create(
      std::move(options), instance_client_provider, cpu_async_executor,
      io_async_executor);
}

absl::StatusOr<RoleCredentialsProviderInterface*>
LibCpioProvider::GetRoleCredentialsProvider() noexcept {
  if (role_credentials_provider_) {
    return role_credentials_provider_.get();
  }

  RoleCredentialsProviderOptions options;
  options.region = GetRegion();
  auto role_credentials_provider = CreateRoleCredentialsProvider(
      std::move(options), &GetInstanceClientProvider(), &GetCpuAsyncExecutor(),
      &GetIoAsyncExecutor());
  if (!role_credentials_provider.ok()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, role_credentials_provider.status(),
              "Failed to initialize role credential provider.");
    return role_credentials_provider.status();
  }
  role_credentials_provider_ = *std::move(role_credentials_provider);
  return role_credentials_provider_.get();
}

AuthTokenProviderInterface& LibCpioProvider::GetAuthTokenProvider() noexcept {
  if (!auth_token_provider_) {
    auth_token_provider_ = AuthTokenProviderFactory::Create(&GetHttp1Client());
  }
  return *auth_token_provider_;
}

const std::string& LibCpioProvider::GetProjectId() noexcept {
  return project_id_;
}

const std::string& LibCpioProvider::GetRegion() noexcept { return region_; }

std::unique_ptr<CpioProviderInterface> CpioProviderFactory::Create(
    CpioOptions options) {
  return std::make_unique<LibCpioProvider>(std::move(options));
}
}  // namespace google::scp::cpio::client_providers
