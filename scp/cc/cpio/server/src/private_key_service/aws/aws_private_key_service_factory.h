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
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/kms_client_provider_interface.h"
#include "cpio/client_providers/interface/private_key_client_provider_interface.h"
#include "cpio/client_providers/interface/private_key_fetcher_provider_interface.h"
#include "cpio/server/src/private_key_service/private_key_service_factory.h"

namespace google::scp::cpio {
/*! @copydoc PrivateKeyServiceFactoryInterface
 */
class AwsPrivateKeyServiceFactory : public PrivateKeyServiceFactory {
 public:
  AwsPrivateKeyServiceFactory(
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : PrivateKeyServiceFactory(config_provider) {}

  core::ExecutionResult Init() noexcept override;

 protected:
  core::ExecutionResultOr<std::shared_ptr<core::ServiceInterface>>
  CreatePrivateKeyFetcher() noexcept override;

  std::shared_ptr<client_providers::RoleCredentialsProviderInterface>
      role_credentials_provider_;
  std::shared_ptr<client_providers::InstanceClientProviderInterface>
      instance_client_;

 private:
  virtual core::ExecutionResultOr<std::shared_ptr<core::ServiceInterface>>
  CreateInstanceClient() noexcept;
  virtual core::ExecutionResultOr<std::shared_ptr<core::ServiceInterface>>
  CreateRoleCredentialsProvider() noexcept;

  core::ExecutionResultOr<std::shared_ptr<core::ServiceInterface>>
  CreateAuthTokenProvider() noexcept override;
};
}  // namespace google::scp::cpio
