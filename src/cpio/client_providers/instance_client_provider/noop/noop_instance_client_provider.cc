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
#include <string>

#include "absl/base/nullability.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "src/public/core/interface/execution_result.h"

using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::InstanceDetails;
using google::cmrt::sdk::instance_service::v1::
    ListInstanceDetailsByEnvironmentRequest;
using google::cmrt::sdk::instance_service::v1::
    ListInstanceDetailsByEnvironmentResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;

namespace {
class NoopInstanceClientProvider : public InstanceClientProviderInterface {
 public:
  absl::Status GetCurrentInstanceResourceName(
      AsyncContext<
          GetCurrentInstanceResourceNameRequest,
          GetCurrentInstanceResourceNameResponse>& /* context */) noexcept
      override {
    return absl::UnimplementedError("");
  }

  absl::Status GetCurrentInstanceResourceNameSync(
      std::string& /* resource_name */) noexcept override {
    return absl::UnimplementedError("");
  }

  absl::Status GetTagsByResourceName(
      AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>&
      /* context */) noexcept override {
    return absl::UnimplementedError("");
  }

  absl::Status GetInstanceDetailsByResourceName(
      AsyncContext<
          GetInstanceDetailsByResourceNameRequest,
          GetInstanceDetailsByResourceNameResponse>& /* context */) noexcept
      override {
    return absl::UnimplementedError("");
  }

  absl::Status ListInstanceDetailsByEnvironment(
      AsyncContext<
          ListInstanceDetailsByEnvironmentRequest,
          ListInstanceDetailsByEnvironmentResponse>& /* context */) noexcept
      override {
    return absl::UnimplementedError("");
  }

  absl::Status GetInstanceDetailsByResourceNameSync(
      std::string_view /* resource_name */,
      InstanceDetails& /* instance_details */) noexcept override {
    return absl::UnimplementedError("");
  }
};
}  // namespace

namespace google::scp::cpio::client_providers {
absl::Nonnull<std::unique_ptr<InstanceClientProviderInterface>>
InstanceClientProviderFactory::Create(
    absl::Nonnull<AuthTokenProviderInterface*> /* auth_token_provider */,
    absl::Nonnull<core::HttpClientInterface*> /* http1_client */,
    absl::Nonnull<core::HttpClientInterface*> /* http2_client */,
    absl::Nonnull<core::AsyncExecutorInterface*> /* async_executor */,
    absl::Nonnull<core::AsyncExecutorInterface*> /* io_async_executor */) {
  return std::make_unique<NoopInstanceClientProvider>();
}
}  // namespace google::scp::cpio::client_providers
