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

#ifndef CPIO_CLIENT_PROVIDERS_INTERFACE_CPIO_PROVIDER_INTERFACE_H_
#define CPIO_CLIENT_PROVIDERS_INTERFACE_CPIO_PROVIDER_INTERFACE_H_

#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "google/protobuf/any.pb.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/interface/http_client_interface.h"
#include "src/cpio/client_providers/interface/auth_token_provider_interface.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "src/public/cpio/interface/type_def.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Provides all required global objects. This class is not thread-safe,
 * but it will only be used for client initialization where only one main
 * process is running.
 *
 */
class CpioProviderInterface {
 public:
  virtual ~CpioProviderInterface() = default;

  /**
   * @brief Gets the global Async Executor.
   * needed.
   *
   * @return cpu_async_executor the CPU Async Executor.
   */
  virtual core::AsyncExecutorInterface& GetCpuAsyncExecutor() noexcept = 0;

  /**
   * @brief Gets the global IO Async Executor.
   * needed.
   *
   * @return io_async_executor the IO Async Executor.
   */
  virtual core::AsyncExecutorInterface& GetIoAsyncExecutor() noexcept = 0;

  /**
   * @brief Get the Http2 Client object.
   * TODO: rename to GetHttp2Client.
   *
   * @return http_client output Http2 Client
   */
  virtual core::HttpClientInterface& GetHttpClient() noexcept = 0;

  /**
   * @brief Get the Http1 Client object.
   *
   * @return http_client output Http1 Client
   */
  virtual core::HttpClientInterface& GetHttp1Client() noexcept = 0;

  /**
   * @brief Gets the InstanceClientProvider.
   *
   * @return instance_client output InstanceClientProvider.
   */
  virtual InstanceClientProviderInterface&
  GetInstanceClientProvider() noexcept = 0;

  /**
   * @brief Gets the Role Credentials Provider.
   *
   * @return credentials_provider output role credentials provider.
   */
  virtual absl::StatusOr<RoleCredentialsProviderInterface*>
  GetRoleCredentialsProvider() noexcept = 0;

  virtual AuthTokenProviderInterface& GetAuthTokenProvider() noexcept = 0;

  /**
   * @brief Gets the Project ID from CpioOptions if originally provided.
   *
   * @return const std::string& of Cloud Project ID.
   */
  virtual const std::string& GetProjectId() noexcept = 0;

  /**
   * @brief Gets the Region from CpioOptions if originally provided.
   *
   * @return const std::string& of Cloud Region.
   */
  virtual const std::string& GetRegion() noexcept = 0;
};

/// Factory to create CpioProvider.
class CpioProviderFactory {
 public:
  /**
   * @brief Creates CpioProvider.
   *
   * @return std::unique_ptr<CpioProviderInterface> CpioProvider.
   */
  static absl::StatusOr<std::unique_ptr<CpioProviderInterface>> Create(
      CpioOptions options);
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_INTERFACE_CPIO_PROVIDER_INTERFACE_H_
