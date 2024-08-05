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

#include "absl/log/check.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/interface/errors.h"
#include "src/core/interface/http_client_interface.h"
#include "src/cpio/client_providers/interface/auth_token_provider_interface.h"
#include "src/cpio/client_providers/interface/cloud_initializer_interface.h"
#include "src/cpio/client_providers/interface/cpio_provider_interface.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/util/status_macro/status_macros.h"

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::common::kZeroUuid;

namespace {
constexpr std::string_view kLibCpioProvider = "LibCpioProvider";
}  // namespace

namespace google::scp::cpio::client_providers {
LibCpioProvider::LibCpioProvider(CpioOptions options)
    : project_id_(std::move(options.project_id)),
<<<<<<< HEAD
      region_(std::move(options.region)) {
=======
      region_(std::move(options.region)),
      cpu_async_executor_(/*thread_count=*/2, /*queue_cap=*/100'000),
      io_async_executor_(/*thread_count=*/2, /*queue_cap=*/100'000),
      http1_client_(&cpu_async_executor_, &io_async_executor_),
      http2_client_(&cpu_async_executor_),
      auth_token_provider_(AuthTokenProviderFactory::Create(&http1_client_)) {
>>>>>>> upstream-3e92e75-3.10.0
  if (options.cloud_init_option == CloudInitOption::kInitInCpio) {
    cloud_initializer_ = CloudInitializerFactory::Create();
    cloud_initializer_->InitCloud();
  }
<<<<<<< HEAD
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

=======
}

void LibCpioProvider::Init() {
  instance_client_provider_ = InstanceClientProviderFactory::Create(
      auth_token_provider_.get(), &http1_client_, &http2_client_,
      &cpu_async_executor_, &io_async_executor_);
}

LibCpioProvider::~LibCpioProvider() {
  if (const ExecutionResult result = http2_client_.Stop();
      !result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, result,
              "Failed to stop http2 client.");
  }
>>>>>>> upstream-3e92e75-3.10.0
  if (cloud_initializer_) {
    cloud_initializer_->ShutdownCloud();
  }
}

HttpClientInterface& LibCpioProvider::GetHttpClient() noexcept {
<<<<<<< HEAD
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
=======
  return http2_client_;
}

HttpClientInterface& LibCpioProvider::GetHttp1Client() noexcept {
  return http1_client_;
}

AsyncExecutorInterface& LibCpioProvider::GetCpuAsyncExecutor() noexcept {
  return cpu_async_executor_;
}

AsyncExecutorInterface& LibCpioProvider::GetIoAsyncExecutor() noexcept {
  return io_async_executor_;
>>>>>>> upstream-3e92e75-3.10.0
}

InstanceClientProviderInterface&
LibCpioProvider::GetInstanceClientProvider() noexcept {
<<<<<<< HEAD
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
=======
  CHECK(instance_client_provider_) << "Init not called.";
  return *instance_client_provider_;
>>>>>>> upstream-3e92e75-3.10.0
}

absl::StatusOr<RoleCredentialsProviderInterface*>
LibCpioProvider::GetRoleCredentialsProvider() noexcept {
  // TODO(b/337035410): Initialize in role_credentials_provider in Init and
  // return ref here.
  absl::MutexLock lock(&mutex_);
  if (!role_credentials_provider_) {
    PS_ASSIGN_OR_RETURN(role_credentials_provider_,
                        RoleCredentialsProviderFactory::Create(
                            RoleCredentialsProviderOptions{.region = region_},
                            &GetInstanceClientProvider(), &cpu_async_executor_,
                            &io_async_executor_));
  }
<<<<<<< HEAD

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
=======
>>>>>>> upstream-3e92e75-3.10.0
  return role_credentials_provider_.get();
}

AuthTokenProviderInterface& LibCpioProvider::GetAuthTokenProvider() noexcept {
<<<<<<< HEAD
  if (!auth_token_provider_) {
    auth_token_provider_ = AuthTokenProviderFactory::Create(&GetHttp1Client());
  }
=======
>>>>>>> upstream-3e92e75-3.10.0
  return *auth_token_provider_;
}

const std::string& LibCpioProvider::GetProjectId() noexcept {
  return project_id_;
}

const std::string& LibCpioProvider::GetRegion() noexcept { return region_; }

absl::StatusOr<std::unique_ptr<CpioProviderInterface>> LibCpioProvider::Create(
    CpioOptions options) {
  // Using `new` to access a non-public constructor.
  auto cpio_provider =
      absl::WrapUnique(new LibCpioProvider(std::move(options)));
  cpio_provider->Init();
  return cpio_provider;
}

absl::StatusOr<std::unique_ptr<CpioProviderInterface>>
CpioProviderFactory::Create(CpioOptions options) {
  return LibCpioProvider::Create(std::move(options));
}
}  // namespace google::scp::cpio::client_providers
