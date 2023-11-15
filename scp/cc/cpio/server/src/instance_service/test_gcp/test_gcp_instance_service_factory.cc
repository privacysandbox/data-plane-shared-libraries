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

#include "cpio/server/src/instance_service/test_gcp/test_gcp_instance_service_factory.h"

#include <memory>
#include <string>

#include "cpio/client_providers/instance_client_provider/test/gcp/test_gcp_instance_client_provider.h"
#include "cpio/server/interface/instance_service/configuration_keys.h"
#include "cpio/server/src/instance_service/test_gcp/configuration_keys.h"
#include "cpio/server/src/service_utils.h"
#include "scp/cc/core/common/uuid/src/uuid.h"

using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::TryReadConfigString;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::TestGcpInstanceClientProvider;
using google::scp::cpio::client_providers::TestInstanceClientOptions;

namespace {
constexpr char kTestGcpInstanceServiceFactory[] =
    "TestGcpInstanceServiceFactory";
constexpr char kDefaultZone[] = "us-central1-a";
constexpr char kDefaultInstanceId[] = "12345678987654321";
}  // namespace

namespace google::scp::cpio {
ExecutionResult TestGcpInstanceServiceFactory::Init() noexcept {
  auto test_options =
      std::dynamic_pointer_cast<TestGcpInstanceServiceFactoryOptions>(options_);
  RETURN_AND_LOG_IF_FAILURE(
      TryReadConfigString(config_provider_,
                          test_options->project_id_config_label, project_id_),
      kTestGcpInstanceServiceFactory, kZeroUuid,
      "Failed to read config for %s.",
      test_options->project_id_config_label.c_str());

  zone_ = kDefaultZone;
  if (!test_options->zone_config_label.empty()) {
    TryReadConfigString(config_provider_, test_options->zone_config_label,
                        zone_);
  }

  instance_id_ = kDefaultInstanceId;
  if (!test_options->instance_id_config_label.empty()) {
    TryReadConfigString(config_provider_,
                        test_options->instance_id_config_label, instance_id_);
  }

  RETURN_AND_LOG_IF_FAILURE(GcpInstanceServiceFactory::Init(),
                            kTestGcpInstanceServiceFactory, kZeroUuid,
                            "Failed to init TestGcpInstanceServiceFactory.");

  return SuccessExecutionResult();
}

std::shared_ptr<InstanceClientProviderInterface>
TestGcpInstanceServiceFactory::CreateInstanceClient() noexcept {
  auto options = std::make_shared<TestInstanceClientOptions>();
  options->project_id = project_id_;
  options->zone = zone_;
  options->instance_id = instance_id_;
  return std::make_shared<TestGcpInstanceClientProvider>(options);
}

}  // namespace google::scp::cpio
