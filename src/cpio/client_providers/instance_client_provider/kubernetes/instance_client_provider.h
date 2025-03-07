/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// NOLINTNEXTLINE(whitespace/line_length)
#ifndef CPIO_CLIENT_PROVIDERS_INSTANCE_CLIENT_PROVIDER_SRC_KUBERNETES_INSTANCE_CLIENT_PROVIDER_H_
// NOLINTNEXTLINE(whitespace/line_length)
#define CPIO_CLIENT_PROVIDERS_INSTANCE_CLIENT_PROVIDER_SRC_KUBERNETES_INSTANCE_CLIENT_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"

namespace google::scp::cpio::client_providers {

// The following tags must be set as environment variables on the pod by
// Kubernetes.
inline constexpr std::string_view kPodResourceIdEnvVar = "POD_RESOURCE_ID";
inline constexpr std::string_view kPodLabelsEnvVar = "POD_LABELS_JSON";
inline constexpr std::string_view kEnvVarValueMissing = "Env var is missing.";
inline constexpr std::string_view kInvalidLabelJson = "Invalid label json.";

// Expects the container to be able to access specific env vars set by the K8s
// Deployment spec.
class KubernetesInstanceClientProvider
    : public InstanceClientProviderInterface {
 public:
  KubernetesInstanceClientProvider();

  absl::Status GetCurrentInstanceResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameResponse>&
          context) noexcept override;

  absl::Status GetTagsByResourceName(
      core::AsyncContext<
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest,
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse>&
          context) noexcept override;

  absl::Status GetInstanceDetailsByResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameResponse>&
          context) noexcept override;

  absl::Status ListInstanceDetailsByEnvironment(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             ListInstanceDetailsByEnvironmentRequest,
                         cmrt::sdk::instance_service::v1::
                             ListInstanceDetailsByEnvironmentResponse>&
          context) noexcept override;

  absl::Status GetCurrentInstanceResourceNameSync(
      std::string& resource_name) noexcept override;

  absl::Status GetInstanceDetailsByResourceNameSync(
      std::string_view resource_name,
      cmrt::sdk::instance_service::v1::InstanceDetails&
          instance_details) noexcept override;
};
}  // namespace google::scp::cpio::client_providers

// NOLINTNEXTLINE(whitespace/line_length)
#endif  // CPIO_CLIENT_PROVIDERS_INSTANCE_CLIENT_PROVIDER_SRC_KUBERNETES_INSTANCE_CLIENT_PROVIDER_H_
