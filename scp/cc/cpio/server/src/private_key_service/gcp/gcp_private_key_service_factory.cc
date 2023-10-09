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

#include "gcp_private_key_service_factory.h"

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cpio/client_providers/auth_token_provider/src/gcp/gcp_auth_token_provider.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cpio/client_providers/interface/kms_client_provider_interface.h"
#include "cpio/client_providers/interface/private_key_fetcher_provider_interface.h"
#include "cpio/client_providers/kms_client_provider/src/gcp/gcp_kms_client_provider.h"
#include "cpio/client_providers/private_key_fetcher_provider/src/gcp/gcp_private_key_fetcher_provider.h"
#include "cpio/server/interface/private_key_service/configuration_keys.h"
#include "cpio/server/src/component_factory/component_factory.h"
#include "cpio/server/src/private_key_service/private_key_service_factory.h"
#include "cpio/server/src/service_utils.h"
#include "public/cpio/interface/private_key_client/type_def.h"
#include "scp/cc/core/common/uuid/src/uuid.h"

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::ServiceInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_PRIVATE_KEY_SERVICE_MISMATCHED_SECONDARY_ENDPOINTS;
using google::scp::cpio::PrivateKeyClientOptions;
using google::scp::cpio::client_providers::AuthTokenProviderInterface;
using google::scp::cpio::client_providers::GcpAuthTokenProvider;
using google::scp::cpio::client_providers::GcpKmsClientProvider;
using google::scp::cpio::client_providers::GcpPrivateKeyFetcherProvider;
using google::scp::cpio::client_providers::KmsClientProviderInterface;
using google::scp::cpio::client_providers::PrivateKeyFetcherProviderInterface;
using std::bind;
using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::vector;

namespace {
constexpr char kGcpPrivateKeyServiceFactory[] = "GcpPrivateKeyServiceFactory";
}  // namespace

namespace google::scp::cpio {
ExecutionResultOr<shared_ptr<ServiceInterface>>
GcpPrivateKeyServiceFactory::CreateAuthTokenProvider() noexcept {
  auth_token_provider_ = make_shared<GcpAuthTokenProvider>(http1_client_);
  return auth_token_provider_;
}

ExecutionResultOr<shared_ptr<ServiceInterface>>
GcpPrivateKeyServiceFactory::CreatePrivateKeyFetcher() noexcept {
  private_key_fetcher_ = make_shared<GcpPrivateKeyFetcherProvider>(
      http2_client_, auth_token_provider_);
  return private_key_fetcher_;
}

ExecutionResultOr<shared_ptr<ServiceInterface>>
GcpPrivateKeyServiceFactory::CreateKmsClient() noexcept {
  kms_client_ = make_shared<GcpKmsClientProvider>();
  return kms_client_;
}

ExecutionResult GcpPrivateKeyServiceFactory::Init() noexcept {
  vector<ComponentCreator> creators(
      {ComponentCreator(
           bind(&GcpPrivateKeyServiceFactory::CreateIoAsyncExecutor, this),
           "IoAsyncExecutor"),
       ComponentCreator(
           bind(&GcpPrivateKeyServiceFactory::CreateCpuAsyncExecutor, this),
           "CpuAsyncExecutor"),
       ComponentCreator(
           bind(&GcpPrivateKeyServiceFactory::CreateHttp1Client, this),
           "Http1Client"),
       ComponentCreator(
           bind(&GcpPrivateKeyServiceFactory::CreateHttp2Client, this),
           "Http2Client"),
       ComponentCreator(
           bind(&GcpPrivateKeyServiceFactory::CreateAuthTokenProvider, this),
           "AuthTokenProvider"),
       ComponentCreator(
           bind(&GcpPrivateKeyServiceFactory::CreatePrivateKeyFetcher, this),
           "PrivateKeyFetcher"),
       ComponentCreator(
           bind(&GcpPrivateKeyServiceFactory::CreateKmsClient, this),
           "KmsClient")});
  component_factory_ = make_shared<ComponentFactory>(move(creators));

  RETURN_AND_LOG_IF_FAILURE(PrivateKeyServiceFactory::Init(),
                            kGcpPrivateKeyServiceFactory, kZeroUuid,
                            "Failed to init PrivateKeyServiceFactory.");

  // Read GCP specific configurations.
  // Read cloudfunction Urls.
  list<string> secondary_private_key_vending_service_cloudfunction_urls;
  RETURN_AND_LOG_IF_FAILURE(
      TryReadConfigStringList(
          config_provider_,
          kGcpSecondaryPrivateKeyVendingServiceCloudfunctionUrls,
          secondary_private_key_vending_service_cloudfunction_urls),
      kGcpPrivateKeyServiceFactory, kZeroUuid,
      "Missing secondary private key vending cloudfunction Urls.");

  if (secondary_private_key_vending_service_cloudfunction_urls.size() !=
      client_options_->secondary_private_key_vending_endpoints.size()) {
    auto execution_result = FailureExecutionResult(
        SC_PRIVATE_KEY_SERVICE_MISMATCHED_SECONDARY_ENDPOINTS);
    SCP_ERROR(
        kGcpPrivateKeyServiceFactory, kZeroUuid, execution_result,
        "Invalid secondary private key vending service cloudfunction urls.");
    return execution_result;
  }

  string primary_private_key_vending_service_cloudfunction_url;
  RETURN_AND_LOG_IF_FAILURE(
      TryReadConfigString(
          config_provider_, kGcpPrimaryPrivateKeyVendingServiceCloudfunctionUrl,
          primary_private_key_vending_service_cloudfunction_url),
      kGcpPrivateKeyServiceFactory, kZeroUuid,
      "Missing primary private key vending cloudfunction Url.");

  // Read WIP providers.
  list<string> secondary_private_key_vending_service_wip_providers;
  RETURN_AND_LOG_IF_FAILURE(
      TryReadConfigStringList(
          config_provider_, kGcpSecondaryPrivateKeyVendingServiceWipProviders,
          secondary_private_key_vending_service_wip_providers),
      kGcpPrivateKeyServiceFactory, kZeroUuid,
      "Missing secondary private key vending WIP providers.");

  if (secondary_private_key_vending_service_wip_providers.size() !=
      client_options_->secondary_private_key_vending_endpoints.size()) {
    auto execution_result = FailureExecutionResult(
        SC_PRIVATE_KEY_SERVICE_MISMATCHED_SECONDARY_ENDPOINTS);
    SCP_ERROR(kGcpPrivateKeyServiceFactory, kZeroUuid, execution_result,
              "Invalid secondary private key vending service WIP providers.");
    return execution_result;
  }

  string primary_private_key_vending_service_wip_provider;
  RETURN_AND_LOG_IF_FAILURE(
      TryReadConfigString(config_provider_,
                          kGcpPrimaryPrivateKeyVendingServiceWipProvider,
                          primary_private_key_vending_service_wip_provider),
      kGcpPrivateKeyServiceFactory, kZeroUuid,
      "Missing primary private key vending WIP provider.");

  client_options_->primary_private_key_vending_endpoint
      .gcp_private_key_vending_service_cloudfunction_url =
      primary_private_key_vending_service_cloudfunction_url;
  client_options_->primary_private_key_vending_endpoint.gcp_wip_provider =
      primary_private_key_vending_service_wip_provider;
  auto secondary_endpoint_it =
      client_options_->secondary_private_key_vending_endpoints.begin();
  auto secondary_cloudfunction_url_it =
      secondary_private_key_vending_service_cloudfunction_urls.begin();
  auto secondary_wip_providers_it =
      secondary_private_key_vending_service_wip_providers.begin();
  while (secondary_endpoint_it !=
         client_options_->secondary_private_key_vending_endpoints.end()) {
    secondary_endpoint_it->gcp_private_key_vending_service_cloudfunction_url =
        *secondary_cloudfunction_url_it;
    secondary_endpoint_it->gcp_wip_provider = *secondary_wip_providers_it;

    ++secondary_endpoint_it;
    ++secondary_cloudfunction_url_it;
    ++secondary_wip_providers_it;
  }

  return SuccessExecutionResult();
}
}  // namespace google::scp::cpio
