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

#ifndef PUBLIC_CPIO_ADAPTERS_INSTANCE_CLIENT_INSTANCE_CLIENT_H_
#define PUBLIC_CPIO_ADAPTERS_INSTANCE_CLIENT_INSTANCE_CLIENT_H_

#include <memory>
#include <string>
#include <utility>

#include "src/cpio/client_providers/interface/cpio_provider_interface.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/instance_client/instance_client_interface.h"
#include "src/public/cpio/proto/instance_service/v1/instance_service.pb.h"

namespace google::scp::cpio {
/*! @copydoc InstanceClientInterface
 */
class InstanceClient : public InstanceClientInterface {
 public:
  explicit InstanceClient(InstanceClientOptions options)
      : options_(std::move(options)) {}

  virtual ~InstanceClient() = default;

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult GetCurrentInstanceResourceName(
      cmrt::sdk::instance_service::v1::GetCurrentInstanceResourceNameRequest
          request,
      Callback<cmrt::sdk::instance_service::v1::
                   GetCurrentInstanceResourceNameResponse>
          callback) noexcept override;

  core::ExecutionResult GetTagsByResourceName(
      cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest request,
      Callback<cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse>
          callback) noexcept override;

  core::ExecutionResult GetInstanceDetailsByResourceName(
      cmrt::sdk::instance_service::v1::GetInstanceDetailsByResourceNameRequest
          request,
      Callback<cmrt::sdk::instance_service::v1::
                   GetInstanceDetailsByResourceNameResponse>
          callback) noexcept override;

  core::ExecutionResult ListInstanceDetailsByEnvironment(
      cmrt::sdk::instance_service::v1::ListInstanceDetailsByEnvironmentRequest
          request,
      Callback<cmrt::sdk::instance_service::v1::
                   ListInstanceDetailsByEnvironmentResponse>
          callback) noexcept override;

 protected:
  virtual void CreateInstanceClientProvider() noexcept;

  client_providers::InstanceClientProviderInterface* instance_client_provider_;

 private:
  InstanceClientOptions options_;
  client_providers::CpioProviderInterface* cpio_;
};
}  // namespace google::scp::cpio

#endif  // PUBLIC_CPIO_ADAPTERS_INSTANCE_CLIENT_INSTANCE_CLIENT_H_
