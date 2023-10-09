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

#include "tee_aws_private_key_service_factory.h"

#include <memory>

#include "cpio/client_providers/kms_client_provider/src/aws/tee_aws_kms_client_provider.h"

using google::scp::core::ExecutionResultOr;
using google::scp::core::ServiceInterface;
using google::scp::cpio::client_providers::KmsClientProviderInterface;
using google::scp::cpio::client_providers::TeeAwsKmsClientProvider;
using std::make_shared;
using std::shared_ptr;

namespace google::scp::cpio {
ExecutionResultOr<shared_ptr<ServiceInterface>>
TeeAwsPrivateKeyServiceFactory::CreateKmsClient() noexcept {
  kms_client_ =
      make_shared<TeeAwsKmsClientProvider>(role_credentials_provider_);
  return kms_client_;
}
}  // namespace google::scp::cpio
