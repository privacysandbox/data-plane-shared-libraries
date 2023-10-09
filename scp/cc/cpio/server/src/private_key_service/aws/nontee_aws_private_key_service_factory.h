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

#include "aws_private_key_service_factory.h"

namespace google::scp::cpio {
/*! @copydoc PrivateKeyServiceFactoryInterface
 */
class NonteeAwsPrivateKeyServiceFactory : public AwsPrivateKeyServiceFactory {
 public:
  NonteeAwsPrivateKeyServiceFactory(
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : AwsPrivateKeyServiceFactory(config_provider) {}

 private:
  core::ExecutionResultOr<std::shared_ptr<core::ServiceInterface>>
  CreateKmsClient() noexcept override;
};
}  // namespace google::scp::cpio
