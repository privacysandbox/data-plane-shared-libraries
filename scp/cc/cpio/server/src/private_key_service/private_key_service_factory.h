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

#pragma once

#include <memory>

#include "core/interface/config_provider_interface.h"
#include "cpio/client_providers/interface/kms_client_provider_interface.h"
#include "cpio/client_providers/interface/private_key_client_provider_interface.h"
#include "cpio/client_providers/interface/private_key_fetcher_provider_interface.h"
#include "cpio/server/interface/component_factory/component_factory_interface.h"
#include "cpio/server/interface/private_key_service/private_key_service_factory_interface.h"
#include "public/cpio/interface/private_key_client/type_def.h"

#include "error_codes.h"

namespace google::scp::cpio {
/**
 * @brief Platform specific factory interface to provide platform specific
 * clients to PrivateKeyService
 *
 */
class PrivateKeyServiceFactory : public PrivateKeyServiceFactoryInterface {
 public:
  explicit PrivateKeyServiceFactory(
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : config_provider_(config_provider) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  std::shared_ptr<client_providers::PrivateKeyClientProviderInterface>
  CreatePrivateKeyClient() noexcept override;

 protected:
  virtual core::ExecutionResult ReadConfigurations() noexcept;
  virtual std::shared_ptr<PrivateKeyClientOptions>
  CreatePrivateKeyClientOptions() noexcept;

  core::ExecutionResultOr<std::shared_ptr<core::ServiceInterface>>
  CreateCpuAsyncExecutor() noexcept;
  core::ExecutionResultOr<std::shared_ptr<core::ServiceInterface>>
  CreateIoAsyncExecutor() noexcept;
  core::ExecutionResultOr<std::shared_ptr<core::ServiceInterface>>
  CreateHttp1Client() noexcept;
  core::ExecutionResultOr<std::shared_ptr<core::ServiceInterface>>
  CreateHttp2Client() noexcept;
  virtual core::ExecutionResultOr<std::shared_ptr<core::ServiceInterface>>
  CreateKmsClient() noexcept = 0;
  virtual core::ExecutionResultOr<std::shared_ptr<core::ServiceInterface>>
  CreateAuthTokenProvider() noexcept = 0;
  virtual core::ExecutionResultOr<std::shared_ptr<core::ServiceInterface>>
  CreatePrivateKeyFetcher() noexcept = 0;

  std::shared_ptr<core::ConfigProviderInterface> config_provider_;
  std::shared_ptr<PrivateKeyClientOptions> client_options_;
  std::shared_ptr<core::HttpClientInterface> http1_client_, http2_client_;
  std::shared_ptr<core::AsyncExecutorInterface> cpu_async_executor_,
      io_async_executor_;
  std::shared_ptr<client_providers::AuthTokenProviderInterface>
      auth_token_provider_;

  std::shared_ptr<client_providers::PrivateKeyFetcherProviderInterface>
      private_key_fetcher_;
  std::shared_ptr<client_providers::KmsClientProviderInterface> kms_client_;

  std::shared_ptr<ComponentFactoryInterface> component_factory_;
};
}  // namespace google::scp::cpio
