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
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/server/interface/instance_service/instance_service_factory_interface.h"

namespace google::scp::cpio {

/*! @copydoc InstanceServiceFactoryInterface
 */
class InstanceServiceFactory : public InstanceServiceFactoryInterface {
 public:
  InstanceServiceFactory(
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      const std::shared_ptr<InstanceServiceFactoryOptions>& options)
      : config_provider_(config_provider), options_(options) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  std::shared_ptr<core::HttpClientInterface> GetHttp1Client() noexcept override;
  std::shared_ptr<core::AsyncExecutorInterface> GetCpuAsynceExecutor() noexcept
      override;
  std::shared_ptr<core::AsyncExecutorInterface> GetIoAsynceExecutor() noexcept
      override;

 protected:
  std::shared_ptr<core::ConfigProviderInterface> config_provider_;

  std::shared_ptr<core::HttpClientInterface> http1_client_;

  std::shared_ptr<client_providers::AuthTokenProviderInterface>
      auth_token_provider_;

  // A thread pool to compute context and issue async calls, or handle
  // callbacks.
  std::shared_ptr<core::AsyncExecutorInterface> cpu_async_executor_,
      io_async_executor_;

  std::shared_ptr<InstanceServiceFactoryOptions> options_;

 private:
  virtual core::ExecutionResultOr<
      std::shared_ptr<client_providers::AuthTokenProviderInterface>>
  CreateAuthTokenProvider() noexcept = 0;
};
}  // namespace google::scp::cpio
