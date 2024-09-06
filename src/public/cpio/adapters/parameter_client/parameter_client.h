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

#ifndef PUBLIC_CPIO_ADAPTERS_PARAMETER_CLIENT_PARAMETER_CLIENT_H_
#define PUBLIC_CPIO_ADAPTERS_PARAMETER_CLIENT_PARAMETER_CLIENT_H_

#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "src/cpio/client_providers/interface/cpio_provider_interface.h"
#include "src/cpio/client_providers/interface/parameter_client_provider_interface.h"
#include "src/public/cpio/interface/parameter_client/parameter_client_interface.h"
#include "src/public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

namespace google::scp::cpio {
/*! @copydoc ParameterClientInterface
 */
class ParameterClient : public ParameterClientInterface {
 public:
  explicit ParameterClient(ParameterClientOptions options)
      : options_(std::move(options)) {}

  virtual ~ParameterClient() = default;

  absl::Status Init() noexcept override;

  absl::Status Run() noexcept override;

  absl::Status Stop() noexcept override;

  absl::Status GetParameter(
      cmrt::sdk::parameter_service::v1::GetParameterRequest request,
      Callback<cmrt::sdk::parameter_service::v1::GetParameterResponse>
          callback) noexcept override;

 protected:
  std::unique_ptr<client_providers::ParameterClientProviderInterface>
      parameter_client_provider_;

 private:
  ParameterClientOptions options_;
  client_providers::CpioProviderInterface* cpio_;
};
}  // namespace google::scp::cpio

#endif  // PUBLIC_CPIO_ADAPTERS_PARAMETER_CLIENT_PARAMETER_CLIENT_H_
