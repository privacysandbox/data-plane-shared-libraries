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

#ifndef CPIO_CLIENT_PROVIDERS_GLOBAL_CPIO_CPIO_PROVIDER_LIB_CPIO_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_GLOBAL_CPIO_CPIO_PROVIDER_LIB_CPIO_PROVIDER_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "google/protobuf/any.pb.h"
#include "src/core/async_executor/async_executor.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/interface/http_client_interface.h"
#include "src/core/interface/message_router_interface.h"
#include "src/core/message_router/message_router.h"
#include "src/cpio/client_providers/interface/auth_token_provider_interface.h"
#include "src/cpio/client_providers/interface/cloud_initializer_interface.h"
#include "src/cpio/client_providers/interface/cpio_provider_interface.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc CpioProviderInterface
 * Provides global objects for native library mode.
 */
class LibCpioProvider : public CpioProviderInterface {
 public:
  explicit LibCpioProvider(CpioOptions options)
      : cpio_options_(std::move(options)), cloud_initializer_(nullptr) {}

  virtual ~LibCpioProvider() = default;

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  absl::StatusOr<core::AsyncExecutorInterface*> GetCpuAsyncExecutor() noexcept
      override;

  absl::StatusOr<core::AsyncExecutorInterface*> GetIoAsyncExecutor() noexcept
      override;

  absl::StatusOr<core::HttpClientInterface*> GetHttpClient() noexcept override;

  absl::StatusOr<core::HttpClientInterface*> GetHttp1Client() noexcept override;

  absl::StatusOr<InstanceClientProviderInterface*>
  GetInstanceClientProvider() noexcept override;

  absl::StatusOr<RoleCredentialsProviderInterface*>
  GetRoleCredentialsProvider() noexcept override;

  absl::StatusOr<AuthTokenProviderInterface*> GetAuthTokenProvider() noexcept
      override;

  const std::string& GetProjectId() noexcept override;

  const std::string& GetRegion() noexcept override;

 protected:
  /// Global CPIO options.
  CpioOptions cpio_options_;
  /// Global cloud initializer.
  std::unique_ptr<CloudInitializerInterface> cloud_initializer_;
  /// Global async executors.
  std::unique_ptr<core::AsyncExecutorInterface> cpu_async_executor_,
      io_async_executor_;
  /// Global http clients.
  std::unique_ptr<core::HttpClientInterface> http1_client_, http2_client_;
  /// Global instance client provider to fetch cloud metadata.
  std::unique_ptr<InstanceClientProviderInterface> instance_client_provider_;
  /// Global role credential provider.
  std::unique_ptr<RoleCredentialsProviderInterface> role_credentials_provider_;
  /// Global auth token provider.
  std::unique_ptr<AuthTokenProviderInterface> auth_token_provider_;

 private:
  virtual std::unique_ptr<RoleCredentialsProviderInterface>
  CreateRoleCredentialsProvider(
      InstanceClientProviderInterface* instance_client_provider,
      core::AsyncExecutorInterface* cpu_async_executor,
      core::AsyncExecutorInterface* io_async_executor) noexcept;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_GLOBAL_CPIO_CPIO_PROVIDER_LIB_CPIO_PROVIDER_H_
