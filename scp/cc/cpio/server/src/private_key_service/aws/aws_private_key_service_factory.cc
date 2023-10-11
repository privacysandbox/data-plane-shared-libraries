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

#include "aws_private_key_service_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "core/interface/service_interface.h"
#include "cpio/client_providers/auth_token_provider/src/aws/aws_auth_token_provider.h"
#include "cpio/client_providers/instance_client_provider/src/aws/aws_instance_client_provider.h"
#include "cpio/client_providers/interface/private_key_client_provider_interface.h"
#include "cpio/client_providers/private_key_client_provider/src/private_key_client_provider.h"
#include "cpio/client_providers/private_key_fetcher_provider/src/aws/aws_private_key_fetcher_provider.h"
#include "cpio/client_providers/role_credentials_provider/src/aws/aws_role_credentials_provider.h"
#include "cpio/server/interface/private_key_service/private_key_service_factory_interface.h"
#include "cpio/server/src/component_factory/component_factory.h"
#include "scp/cc/core/common/uuid/src/uuid.h"

using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::HttpClientInterface;
using google::scp::core::ServiceInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::PrivateKeyClientOptions;
using google::scp::cpio::client_providers::AuthTokenProviderInterface;
using google::scp::cpio::client_providers::AwsAuthTokenProvider;
using google::scp::cpio::client_providers::AwsInstanceClientProvider;
using google::scp::cpio::client_providers::AwsPrivateKeyFetcherProvider;
using google::scp::cpio::client_providers::AwsRoleCredentialsProvider;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::KmsClientProviderInterface;
using google::scp::cpio::client_providers::PrivateKeyFetcherProviderInterface;
using google::scp::cpio::client_providers::RoleCredentialsProviderInterface;
using std::bind;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;

namespace {
constexpr char kAwsPrivateKeyServiceFactory[] = "AwsPrivateKeyServiceFactory";
}  // namespace

namespace google::scp::cpio {
ExecutionResultOr<shared_ptr<ServiceInterface>>
AwsPrivateKeyServiceFactory::CreateAuthTokenProvider() noexcept {
  auth_token_provider_ = make_shared<AwsAuthTokenProvider>(http1_client_);
  return auth_token_provider_;
}

ExecutionResultOr<shared_ptr<ServiceInterface>>
AwsPrivateKeyServiceFactory::CreateInstanceClient() noexcept {
  instance_client_ = make_shared<AwsInstanceClientProvider>(
      auth_token_provider_, http1_client_, cpu_async_executor_,
      io_async_executor_);
  return instance_client_;
}

ExecutionResultOr<shared_ptr<ServiceInterface>>
AwsPrivateKeyServiceFactory::CreatePrivateKeyFetcher() noexcept {
  private_key_fetcher_ = make_shared<AwsPrivateKeyFetcherProvider>(
      http2_client_, role_credentials_provider_);
  return private_key_fetcher_;
}

ExecutionResultOr<shared_ptr<ServiceInterface>>
AwsPrivateKeyServiceFactory::CreateRoleCredentialsProvider() noexcept {
  role_credentials_provider_ = make_shared<AwsRoleCredentialsProvider>(
      instance_client_, cpu_async_executor_, io_async_executor_);
  return role_credentials_provider_;
}

ExecutionResult AwsPrivateKeyServiceFactory::Init() noexcept {
  RETURN_AND_LOG_IF_FAILURE(ReadConfigurations(), kAwsPrivateKeyServiceFactory,
                            kZeroUuid, "Failed to read configurations");

  std::vector<ComponentCreator> creators(
      {ComponentCreator(
           bind(&AwsPrivateKeyServiceFactory::CreateIoAsyncExecutor, this),
           "IoAsyncExecutor"),
       ComponentCreator(
           bind(&AwsPrivateKeyServiceFactory::CreateCpuAsyncExecutor, this),
           "CpuAsyncExecutor"),
       ComponentCreator(
           bind(&AwsPrivateKeyServiceFactory::CreateHttp1Client, this),
           "Http1Client"),
       ComponentCreator(
           bind(&AwsPrivateKeyServiceFactory::CreateHttp2Client, this),
           "Http2Client"),
       ComponentCreator(
           bind(&AwsPrivateKeyServiceFactory::CreateAuthTokenProvider, this),
           "AuthTokenProvider"),
       ComponentCreator(
           bind(&AwsPrivateKeyServiceFactory::CreateInstanceClient, this),
           "InstanceClient"),
       ComponentCreator(
           bind(&AwsPrivateKeyServiceFactory::CreateRoleCredentialsProvider,
                this),
           "RoleCredentialsProvider"),
       ComponentCreator(
           bind(&AwsPrivateKeyServiceFactory::CreatePrivateKeyFetcher, this),
           "PrivateKeyFetcher"),
       ComponentCreator(
           bind(&AwsPrivateKeyServiceFactory::CreateKmsClient, this),
           "KmsClient")});
  component_factory_ = make_shared<ComponentFactory>(move(creators));

  RETURN_AND_LOG_IF_FAILURE(PrivateKeyServiceFactory::Init(),
                            kAwsPrivateKeyServiceFactory, kZeroUuid,
                            "Failed to init PrivateKeyServiceFactory.");

  return SuccessExecutionResult();
}
}  // namespace google::scp::cpio
