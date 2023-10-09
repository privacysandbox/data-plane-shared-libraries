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

#include "aws_instance_service_factory.h"

#include <memory>

#include "core/async_executor/src/async_executor.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/auth_token_provider/src/aws/aws_auth_token_provider.h"
#include "cpio/client_providers/instance_client_provider/src/aws/aws_instance_client_provider.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/server/interface/instance_service/configuration_keys.h"
#include "cpio/server/interface/instance_service/instance_service_factory_interface.h"
#include "cpio/server/src/service_utils.h"
#include "scp/cc/core/common/uuid/src/uuid.h"

#include "error_codes.h"

using google::scp::core::AsyncExecutor;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_AWS_INSTANCE_SERVICE_FACTORY_HTTP2_CLIENT_NOT_FOUND;
using google::scp::cpio::TryReadConfigInt;
using google::scp::cpio::client_providers::AuthTokenProviderInterface;
using google::scp::cpio::client_providers::AwsAuthTokenProvider;
using google::scp::cpio::client_providers::AwsInstanceClientProvider;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using std::make_shared;
using std::shared_ptr;

namespace google::scp::cpio {
ExecutionResultOr<shared_ptr<AuthTokenProviderInterface>>
AwsInstanceServiceFactory::CreateAuthTokenProvider() noexcept {
  return make_shared<AwsAuthTokenProvider>(http1_client_);
}

std::shared_ptr<InstanceClientProviderInterface>
AwsInstanceServiceFactory::CreateInstanceClient() noexcept {
  return make_shared<AwsInstanceClientProvider>(
      auth_token_provider_, http1_client_, cpu_async_executor_,
      io_async_executor_);
}

ExecutionResultOr<shared_ptr<HttpClientInterface>>
AwsInstanceServiceFactory::GetHttp2Client() noexcept {
  return FailureExecutionResult(
      SC_AWS_INSTANCE_SERVICE_FACTORY_HTTP2_CLIENT_NOT_FOUND);
}
}  // namespace google::scp::cpio
