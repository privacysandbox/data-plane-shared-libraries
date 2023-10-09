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

#include "instance_service_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "core/async_executor/src/async_executor.h"
#include "core/curl_client/src/http1_curl_client.h"
#include "core/interface/config_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/server/interface/instance_service/instance_service_factory_interface.h"
#include "cpio/server/src/service_utils.h"
#include "scp/cc/core/common/uuid/src/uuid.h"

using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::Http1CurlClient;
using google::scp::core::HttpClientInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::TryReadConfigInt;
using google::scp::cpio::TryReadConfigString;
using google::scp::cpio::TryReadConfigStringList;
using google::scp::cpio::client_providers::AuthTokenProviderInterface;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace {
constexpr char kInstanceServiceFactory[] = "InstanceServiceFactory";
constexpr int kDefaultCpuThreadCount = 2;
constexpr int kDefaultCpuThreadPoolQueueCap = 100000;
constexpr int kDefaultIoThreadCount = 2;
constexpr int kDefaultIoThreadPoolQueueCap = 100000;
}  // namespace

namespace google::scp::cpio {
ExecutionResult InstanceServiceFactory::Init() noexcept {
  int32_t cpu_thread_count = kDefaultCpuThreadCount;
  TryReadConfigInt(config_provider_,
                   options_->cpu_async_executor_thread_count_config_label,
                   cpu_thread_count);
  int32_t cpu_thread_pool_queue_cap = kDefaultCpuThreadPoolQueueCap;
  TryReadConfigInt(config_provider_,
                   options_->cpu_async_executor_queue_cap_config_label,
                   cpu_thread_pool_queue_cap);
  cpu_async_executor_ =
      make_shared<AsyncExecutor>(cpu_thread_count, cpu_thread_pool_queue_cap);

  int32_t io_thread_count = kDefaultIoThreadCount;
  TryReadConfigInt(config_provider_,
                   options_->io_async_executor_thread_count_config_label,
                   io_thread_count);
  int32_t io_thread_pool_queue_cap = kDefaultIoThreadPoolQueueCap;
  TryReadConfigInt(config_provider_,
                   options_->io_async_executor_queue_cap_config_label,
                   io_thread_pool_queue_cap);
  io_async_executor_ =
      make_shared<AsyncExecutor>(io_thread_count, io_thread_pool_queue_cap);

  http1_client_ =
      make_shared<Http1CurlClient>(cpu_async_executor_, io_async_executor_);

  auto auth_token_provider_or = CreateAuthTokenProvider();
  if (!auth_token_provider_or.Successful()) {
    SCP_ERROR(kInstanceServiceFactory, kZeroUuid,
              auth_token_provider_or.result(),
              "Failed to create auth token provider");
    return auth_token_provider_or.result();
  }
  auth_token_provider_ = move(*auth_token_provider_or);

  auto execution_result = cpu_async_executor_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kInstanceServiceFactory, kZeroUuid, execution_result,
              "Failed to init CPU bound AsyncExecutor");
    return execution_result;
  }

  execution_result = io_async_executor_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kInstanceServiceFactory, kZeroUuid, execution_result,
              "Failed to init IO bound AsyncExecutor");
    return execution_result;
  }

  execution_result = http1_client_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kInstanceServiceFactory, kZeroUuid, execution_result,
              "Failed to init Http1Client");
    return execution_result;
  }

  execution_result = auth_token_provider_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kInstanceServiceFactory, kZeroUuid, execution_result,
              "Failed to init AuthTokenProvider");
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult InstanceServiceFactory::Run() noexcept {
  auto execution_result = cpu_async_executor_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kInstanceServiceFactory, kZeroUuid, execution_result,
              "Failed to run CPU bound AsyncExecutor");
    return execution_result;
  }

  execution_result = io_async_executor_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kInstanceServiceFactory, kZeroUuid, execution_result,
              "Failed to run IO bound AsyncExecutor");
    return execution_result;
  }

  execution_result = http1_client_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kInstanceServiceFactory, kZeroUuid, execution_result,
              "Failed to run HttpClient1");
    return execution_result;
  }

  execution_result = auth_token_provider_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kInstanceServiceFactory, kZeroUuid, execution_result,
              "Failed to run AuthTokenProvider.");
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult InstanceServiceFactory::Stop() noexcept {
  auto execution_result = auth_token_provider_->Stop();
  if (!execution_result.Successful()) {
    SCP_ERROR(kInstanceServiceFactory, kZeroUuid, execution_result,
              "Failed to stop AuthTokenProvider.");
    return execution_result;
  }

  execution_result = http1_client_->Stop();
  if (!execution_result.Successful()) {
    SCP_ERROR(kInstanceServiceFactory, kZeroUuid, execution_result,
              "Failed to stop Http1Client");
    return execution_result;
  }

  execution_result = io_async_executor_->Stop();
  if (!execution_result.Successful()) {
    SCP_ERROR(kInstanceServiceFactory, kZeroUuid, execution_result,
              "Failed to stop IO bound AsyncExecutor");
    return execution_result;
  }

  execution_result = cpu_async_executor_->Stop();
  if (!execution_result.Successful()) {
    SCP_ERROR(kInstanceServiceFactory, kZeroUuid, execution_result,
              "Failed to stop CPU bound AsyncExecutor");
    return execution_result;
  }

  return SuccessExecutionResult();
}

shared_ptr<HttpClientInterface>
InstanceServiceFactory::GetHttp1Client() noexcept {
  return http1_client_;
}

shared_ptr<AsyncExecutorInterface>
InstanceServiceFactory::GetCpuAsynceExecutor() noexcept {
  return cpu_async_executor_;
}

shared_ptr<AsyncExecutorInterface>
InstanceServiceFactory::GetIoAsynceExecutor() noexcept {
  return io_async_executor_;
}
}  // namespace google::scp::cpio
