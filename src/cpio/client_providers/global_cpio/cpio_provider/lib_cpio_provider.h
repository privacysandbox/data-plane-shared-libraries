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

#include "absl/base/thread_annotations.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "src/core/async_executor/async_executor.h"
#include "src/core/curl_client/http1_curl_client.h"
#include "src/core/http2_client/http2_client.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/interface/http_client_interface.h"
#include "src/cpio/client_providers/interface/auth_token_provider_interface.h"
#include "src/cpio/client_providers/interface/cloud_initializer_interface.h"
#include "src/cpio/client_providers/interface/cpio_provider_interface.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc CpioProviderInterface
 * Provides global objects for native library mode.
 */
class LibCpioProvider : public CpioProviderInterface {
 public:
  static absl::StatusOr<std::unique_ptr<CpioProviderInterface>> Create(
      CpioOptions options);

  ~LibCpioProvider() override;

  LibCpioProvider(LibCpioProvider&&) = delete;
  LibCpioProvider& operator=(LibCpioProvider&&) = delete;

  core::AsyncExecutorInterface& GetCpuAsyncExecutor() noexcept override;

  core::AsyncExecutorInterface& GetIoAsyncExecutor() noexcept override;

  core::HttpClientInterface& GetHttpClient() noexcept override;

  core::HttpClientInterface& GetHttp1Client() noexcept override;

  InstanceClientProviderInterface& GetInstanceClientProvider() noexcept
      override;

  absl::StatusOr<RoleCredentialsProviderInterface*>
  GetRoleCredentialsProvider() noexcept override ABSL_LOCKS_EXCLUDED(mutex_);

  AuthTokenProviderInterface& GetAuthTokenProvider() noexcept override;

  const std::string& GetProjectId() noexcept override;

  const std::string& GetRegion() noexcept override;

 private:
  void Init();

  /// Global CPIO options.
  std::string project_id_;
  std::string region_;
  /// Global cloud initializer.
  std::unique_ptr<CloudInitializerInterface> cloud_initializer_;
  /// Global async executors.
  core::AsyncExecutor cpu_async_executor_, io_async_executor_;
  /// Global http clients.
  core::Http1CurlClient http1_client_;
  core::HttpClient http2_client_;
  /// Global auth token provider.
  std::unique_ptr<AuthTokenProviderInterface> auth_token_provider_;

 protected:
  explicit LibCpioProvider(CpioOptions options);

  /// Global instance client provider to fetch cloud metadata.
  std::unique_ptr<InstanceClientProviderInterface> instance_client_provider_;
  /// Global role credential provider.
  std::unique_ptr<RoleCredentialsProviderInterface> role_credentials_provider_
      ABSL_GUARDED_BY(mutex_);
  // TODO(b/337035410): Init role_credentials_provider_ in private Init method
  // and remove lock.
  absl::Mutex mutex_;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_GLOBAL_CPIO_CPIO_PROVIDER_LIB_CPIO_PROVIDER_H_
