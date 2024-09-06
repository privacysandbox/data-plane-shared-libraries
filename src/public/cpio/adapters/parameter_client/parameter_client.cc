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

#include "parameter_client.h"

#include <memory>
#include <utility>

#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/public/cpio/adapters/common/adapter_utils.h"
#include "src/public/cpio/proto/parameter_service/v1/parameter_service.pb.h"
#include "src/util/status_macro/status_macros.h"

using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::ParameterClientProviderFactory;
using google::scp::cpio::client_providers::ParameterClientProviderInterface;

namespace google::scp::cpio {
absl::Status ParameterClient::Init() noexcept {
  cpio_ = &GlobalCpio::GetGlobalCpio();
  ParameterClientOptions options = options_;
  if (options.project_id.empty()) {
    options.project_id = cpio_->GetProjectId();
  }
  if (options.region.empty()) {
    options.region = cpio_->GetRegion();
  }

  // TODO(b/321117161): Replace CPU w/ IO executor.
  PS_ASSIGN_OR_RETURN(
      parameter_client_provider_,
      ParameterClientProviderFactory::Create(
          std::move(options), &cpio_->GetInstanceClientProvider(),
          /*cpu_async_executor=*/&cpio_->GetCpuAsyncExecutor(),
          /*io_async_executor=*/&cpio_->GetCpuAsyncExecutor()));
  return absl::OkStatus();
}

absl::Status ParameterClient::Run() noexcept { return absl::OkStatus(); }

absl::Status ParameterClient::Stop() noexcept { return absl::OkStatus(); }

absl::Status ParameterClient::GetParameter(
    GetParameterRequest request,
    Callback<GetParameterResponse> callback) noexcept {
  return Execute<GetParameterRequest, GetParameterResponse>(
      absl::bind_front(&ParameterClientProviderInterface::GetParameter,
                       parameter_client_provider_.get()),
      std::move(request), std::move(callback));
}

std::unique_ptr<ParameterClientInterface> ParameterClientFactory::Create(
    ParameterClientOptions options) {
  return std::make_unique<ParameterClient>(std::move(options));
}
}  // namespace google::scp::cpio
