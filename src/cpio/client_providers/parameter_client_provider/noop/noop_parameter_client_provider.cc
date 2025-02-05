/*
 * Copyright 2025 Google LLC
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

#include <memory>

#include "absl/base/nullability.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/interface/parameter_client_provider_interface.h"
#include "src/public/core/interface/execution_result.h"

using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::cpio::client_providers::ParameterClientProviderInterface;

namespace {
class NoopParameterClientProvider : public ParameterClientProviderInterface {
 public:
  absl::Status GetParameter(
      AsyncContext<GetParameterRequest,
                   GetParameterResponse>& /* context */) noexcept override {
    return absl::UnimplementedError("");
  }
};
}  // namespace

namespace google::scp::cpio::client_providers {
absl::StatusOr<std::unique_ptr<ParameterClientProviderInterface>>
ParameterClientProviderFactory::Create(
    ParameterClientOptions /* options */,
    absl::Nonnull<
        InstanceClientProviderInterface*> /* instance_client_provider */,
    absl::Nonnull<core::AsyncExecutorInterface*> /* cpu_async_executor */,
    absl::Nonnull<core::AsyncExecutorInterface*> /* io_async_executor */) {
  return std::make_unique<NoopParameterClientProvider>();
}
}  // namespace google::scp::cpio::client_providers
