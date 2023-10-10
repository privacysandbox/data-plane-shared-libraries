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

#include "gcp_instance_service_factory.h"

#include <memory>
#include <utility>

#include "core/async_executor/src/async_executor.h"
#include "core/http2_client/src/http2_client.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/auth_token_provider/src/gcp/gcp_auth_token_provider.h"
#include "cpio/client_providers/instance_client_provider/src/gcp/gcp_instance_client_provider.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/server/interface/instance_service/configuration_keys.h"
#include "cpio/server/interface/instance_service/instance_service_factory_interface.h"
#include "cpio/server/src/service_utils.h"
#include "scp/cc/core/common/uuid/src/uuid.h"

using google::scp::core::AsyncExecutor;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::HttpClient;
using google::scp::core::HttpClientInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::TryReadConfigInt;
using google::scp::cpio::client_providers::AuthTokenProviderInterface;
using google::scp::cpio::client_providers::GcpAuthTokenProvider;
using google::scp::cpio::client_providers::GcpInstanceClientProvider;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using std::make_shared;
using std::shared_ptr;

namespace {
constexpr char kGcpInstanceServiceFactory[] = "GcpInstanceServiceFactory";
}  // namespace

namespace google::scp::cpio {
ExecutionResultOr<shared_ptr<AuthTokenProviderInterface>>
GcpInstanceServiceFactory::CreateAuthTokenProvider() noexcept {
  return make_shared<GcpAuthTokenProvider>(http1_client_);
}

ExecutionResult GcpInstanceServiceFactory::Init() noexcept {
  auto execution_result = InstanceServiceFactory::Init();
  RETURN_IF_FAILURE(execution_result);

  http2_client_ = make_shared<HttpClient>(cpu_async_executor_);
  execution_result = http2_client_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kGcpInstanceServiceFactory, kZeroUuid, execution_result,
              "Failed to init Http2Client");
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult GcpInstanceServiceFactory::Run() noexcept {
  auto execution_result = InstanceServiceFactory::Run();
  RETURN_IF_FAILURE(execution_result);

  execution_result = http2_client_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kGcpInstanceServiceFactory, kZeroUuid, execution_result,
              "Failed to run Http2Client");
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult GcpInstanceServiceFactory::Stop() noexcept {
  auto execution_result = InstanceServiceFactory::Stop();
  RETURN_IF_FAILURE(execution_result);

  execution_result = http2_client_->Stop();
  if (!execution_result.Successful()) {
    SCP_ERROR(kGcpInstanceServiceFactory, kZeroUuid, execution_result,
              "Failed to stop Http2Client");
    return execution_result;
  }

  return SuccessExecutionResult();
}

std::shared_ptr<InstanceClientProviderInterface>
GcpInstanceServiceFactory::CreateInstanceClient() noexcept {
  return make_shared<GcpInstanceClientProvider>(auth_token_provider_,
                                                http1_client_, http2_client_);
}

ExecutionResultOr<shared_ptr<HttpClientInterface>>
GcpInstanceServiceFactory::GetHttp2Client() noexcept {
  return http2_client_;
}
}  // namespace google::scp::cpio
