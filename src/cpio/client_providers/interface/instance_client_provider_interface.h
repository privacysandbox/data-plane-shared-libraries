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

#ifndef CPIO_CLIENT_PROVIDERS_INTERFACE_INSTANCE_CLIENT_PROVIDER_INTERFACE_H_
#define CPIO_CLIENT_PROVIDERS_INTERFACE_INSTANCE_CLIENT_PROVIDER_INTERFACE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "src/core/interface/http_client_interface.h"
#include "src/core/interface/service_interface.h"
#include "src/cpio/client_providers/interface/auth_token_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/instance_service/v1/instance_service.pb.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Interface responsible for fetching instance data.
 */
class InstanceClientProviderInterface {
 public:
  virtual ~InstanceClientProviderInterface() = default;

  /**
   * @brief Get the Current Instance Resource Name object
   *
   * @param context context of the operation.
   * @return absl::Status result of the operation.
   */
  virtual absl::Status GetCurrentInstanceResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameResponse>&
          context) noexcept = 0;

  /**
   * @brief Get the Current Instance Resource  object synchronously.
   *
   * @param resource_name[out] the resource name of current instance.
   * @return absl::Status
   */
  virtual absl::Status GetCurrentInstanceResourceNameSync(
      std::string& resource_name) noexcept = 0;

  /**
   * @brief Get the Tags By Resource Name object
   *
   * @param context context of the operation.
   * @return absl::Status result of the operation.
   */
  virtual absl::Status GetTagsByResourceName(
      core::AsyncContext<
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest,
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse>&
          context) noexcept = 0;

  /**
   * @brief Get the Instance Details By Resource Name object
   *
   * @param context context of the operation.
   * @return absl::Status result of the operation.
   */
  virtual absl::Status GetInstanceDetailsByResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameResponse>&
          context) noexcept = 0;

  /**
   * @brief List Instances By Environment
   *
   * @param context context of the operation.
   * @return absl::Status result of the operation.
   */
  virtual absl::Status ListInstanceDetailsByEnvironment(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             ListInstanceDetailsByEnvironmentRequest,
                         cmrt::sdk::instance_service::v1::
                             ListInstanceDetailsByEnvironmentResponse>&
          context) noexcept = 0;

  /**
   * @brief Get the Instance Details By Resource Name object synchronously.
   *
   * @param resource_name the given resource name
   * @param instance_details the details of the given instance.
   * @return absl::Status
   */
  virtual absl::Status GetInstanceDetailsByResourceNameSync(
      std::string_view resource_name,
      cmrt::sdk::instance_service::v1::InstanceDetails&
          instance_details) noexcept = 0;
};

class InstanceClientProviderFactory {
 public:
  /**
   * @brief Factory to create InstanceClientProvider.
   *
   * @param http_client instance of http_client.
   * @return std::unique_ptr<InstanceClientProviderInterface>
   */
  static absl::Nonnull<std::unique_ptr<InstanceClientProviderInterface>> Create(
      absl::Nonnull<AuthTokenProviderInterface*> auth_token_provider,
      absl::Nonnull<core::HttpClientInterface*> http1_client,
      absl::Nonnull<core::HttpClientInterface*> http2_client,
      absl::Nonnull<core::AsyncExecutorInterface*> async_executor,
      absl::Nonnull<core::AsyncExecutorInterface*> io_async_executor);
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_INTERFACE_INSTANCE_CLIENT_PROVIDER_INTERFACE_H_
