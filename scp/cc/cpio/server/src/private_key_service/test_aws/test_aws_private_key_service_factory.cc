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

#include "test_aws_private_key_service_factory.h"

#include <memory>
#include <string>

#include "core/interface/config_provider_interface.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/instance_client_provider/test/aws/test_aws_instance_client_provider.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/kms_client_provider/test/aws/test_aws_kms_client_provider.h"
#include "cpio/client_providers/private_key_fetcher_provider/test/aws/test_aws_private_key_fetcher_provider.h"
#include "cpio/client_providers/role_credentials_provider/test/aws/test_aws_role_credentials_provider.h"
#include "cpio/server/src/private_key_service/aws/nontee_aws_private_key_service_factory.h"
#include "cpio/server/src/service_utils.h"
#include "public/cpio/test/private_key_client/test_aws_private_key_client_options.h"
#include "scp/cc/core/common/uuid/src/uuid.h"

#include "test_configuration_keys.h"

using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::ServiceInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::kTestAwsPrivateKeyClientRegion;
using google::scp::cpio::TryReadConfigString;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::KmsClientProviderInterface;
using google::scp::cpio::client_providers::PrivateKeyFetcherProviderInterface;
using google::scp::cpio::client_providers::RoleCredentialsProviderInterface;
using google::scp::cpio::client_providers::TestAwsKmsClientProvider;
using google::scp::cpio::client_providers::TestAwsPrivateKeyFetcherProvider;
using google::scp::cpio::client_providers::TestAwsRoleCredentialsProvider;
using google::scp::cpio::client_providers::
    TestAwsRoleCredentialsProviderOptions;
using google::scp::cpio::client_providers::TestInstanceClientOptions;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;

namespace {
constexpr char kDefaultRegion[] = "us-east-1";
}  // namespace

namespace google::scp::cpio {
ExecutionResult TestAwsPrivateKeyServiceFactory::ReadConfigurations() noexcept {
  RETURN_IF_FAILURE(PrivateKeyServiceFactory::ReadConfigurations());

  auto test_options =
      dynamic_pointer_cast<TestAwsPrivateKeyClientOptions>(client_options_);
  TryReadConfigString(config_provider_,
                      kTestAwsPrivateKeyClientKmsEndpointOverride,
                      *test_options->kms_endpoint_override);
  TryReadConfigString(config_provider_,
                      kTestAwsPrivateKeyClientStsEndpointOverride,
                      *test_options->sts_endpoint_override);
  TryReadConfigBool(config_provider_, kTestAwsPrivateKeyClientIntegrationTest,
                    test_options->is_integration_test);

  return SuccessExecutionResult();
}

shared_ptr<PrivateKeyClientOptions>
TestAwsPrivateKeyServiceFactory::CreatePrivateKeyClientOptions() noexcept {
  return make_shared<TestAwsPrivateKeyClientOptions>();
}

ExecutionResultOr<shared_ptr<ServiceInterface>>
TestAwsPrivateKeyServiceFactory::CreateInstanceClient() noexcept {
  string region = kDefaultRegion;
  TryReadConfigString(config_provider_, kTestAwsPrivateKeyClientRegion, region);
  auto test_instance_client_options = make_shared<TestInstanceClientOptions>();
  test_instance_client_options->region = region;
  instance_client_ = make_shared<
      google::scp::cpio::client_providers::TestAwsInstanceClientProvider>(
      test_instance_client_options);
  return instance_client_;
}

ExecutionResultOr<shared_ptr<ServiceInterface>>
TestAwsPrivateKeyServiceFactory::CreateKmsClient() noexcept {
  auto test_kms_client_options = make_shared<TestAwsKmsClientOptions>();
  test_kms_client_options->kms_endpoint_override =
      dynamic_pointer_cast<TestAwsPrivateKeyClientOptions>(client_options_)
          ->kms_endpoint_override;

  kms_client_ = make_shared<TestAwsKmsClientProvider>(
      test_kms_client_options, role_credentials_provider_, io_async_executor_);
  return kms_client_;
}

ExecutionResultOr<shared_ptr<ServiceInterface>>
TestAwsPrivateKeyServiceFactory::CreateRoleCredentialsProvider() noexcept {
  auto test_role_credentials_provider_options =
      make_shared<TestAwsRoleCredentialsProviderOptions>();
  test_role_credentials_provider_options->sts_endpoint_override =
      dynamic_pointer_cast<TestAwsPrivateKeyClientOptions>(client_options_)
          ->sts_endpoint_override;

  role_credentials_provider_ = make_shared<TestAwsRoleCredentialsProvider>(
      test_role_credentials_provider_options, instance_client_,
      cpu_async_executor_, io_async_executor_);
  return role_credentials_provider_;
}

ExecutionResultOr<shared_ptr<ServiceInterface>>
TestAwsPrivateKeyServiceFactory::CreatePrivateKeyFetcher() noexcept {
  if (dynamic_pointer_cast<TestAwsPrivateKeyClientOptions>(client_options_)
          ->is_integration_test) {
    private_key_fetcher_ = make_shared<TestAwsPrivateKeyFetcherProvider>(
        http2_client_, role_credentials_provider_);
    return private_key_fetcher_;
  }
  return AwsPrivateKeyServiceFactory::CreatePrivateKeyFetcher();
}
}  // namespace google::scp::cpio
