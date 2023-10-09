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

#include "core/interface/async_executor_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/server/interface/instance_service/instance_service_factory_interface.h"
#include "cpio/server/src/instance_service/instance_service_factory.h"

namespace google::scp::cpio {
/*! @copydoc InstanceServiceFactory
 */
class GcpInstanceServiceFactory : public InstanceServiceFactory {
 public:
  using InstanceServiceFactory::InstanceServiceFactory;

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  std::shared_ptr<client_providers::InstanceClientProviderInterface>
  CreateInstanceClient() noexcept override;

  core::ExecutionResultOr<std::shared_ptr<core::HttpClientInterface>>
  GetHttp2Client() noexcept override;

 private:
  std::shared_ptr<core::HttpClientInterface> http2_client_;

  core::ExecutionResultOr<
      std::shared_ptr<client_providers::AuthTokenProviderInterface>>
  CreateAuthTokenProvider() noexcept override;
};
}  // namespace google::scp::cpio
