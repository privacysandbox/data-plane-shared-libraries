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

#include "nontee_aws_private_key_service_factory.h"

#include <memory>

#include "cpio/client_providers/kms_client_provider/src/aws/nontee_aws_kms_client_provider.h"

using google::scp::core::ExecutionResultOr;
using google::scp::core::ServiceInterface;
using google::scp::cpio::client_providers::KmsClientProviderInterface;
using google::scp::cpio::client_providers::NonteeAwsKmsClientProvider;

namespace google::scp::cpio {
ExecutionResultOr<std::shared_ptr<ServiceInterface>>
NonteeAwsPrivateKeyServiceFactory::CreateKmsClient() noexcept {
  kms_client_ = std::make_shared<NonteeAwsKmsClientProvider>(
      role_credentials_provider_, io_async_executor_);
  return kms_client_;
}
}  // namespace google::scp::cpio
