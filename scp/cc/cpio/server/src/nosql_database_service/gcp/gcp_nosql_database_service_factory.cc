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

#include "gcp_nosql_database_service_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "cpio/client_providers/interface/nosql_database_client_provider_interface.h"
#include "cpio/client_providers/nosql_database_client_provider/src/gcp/gcp_nosql_database_client_provider.h"
#include "cpio/server/interface/nosql_database_service/configuration_keys.h"
#include "cpio/server/interface/nosql_database_service/nosql_database_service_factory_interface.h"
#include "cpio/server/src/instance_service/gcp/gcp_instance_service_factory.h"
#include "cpio/server/src/service_utils.h"
#include "scp/cc/core/common/uuid/src/uuid.h"

using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::TryReadConfigString;
using google::scp::cpio::client_providers::GcpNoSQLDatabaseClientProvider;
using google::scp::cpio::client_providers::NoSQLDatabaseClientProviderInterface;
using std::make_shared;
using std::shared_ptr;

namespace {
constexpr char kGcpNoSQLDatabaseServiceFactory[] =
    "GcpNoSQLDatabaseServiceFactory";
}  // namespace

namespace google::scp::cpio {
shared_ptr<InstanceServiceFactoryInterface>
GcpNoSQLDatabaseServiceFactory::CreateInstanceServiceFactory() noexcept {
  return make_shared<GcpInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

ExecutionResult GcpNoSQLDatabaseServiceFactory::Init() noexcept {
  NoSQLDatabaseServiceFactory::Init();

  auto execution_result = TryReadConfigString(
      config_provider_, kGcpNoSQLDatabaseClientSpannerInstanceName,
      client_options_->instance_name);
  if (!execution_result.Successful()) {
    SCP_ERROR(kGcpNoSQLDatabaseServiceFactory, kZeroUuid, execution_result,
              "Failed to get Spanner Instance name");
    return execution_result;
  }
  execution_result = TryReadConfigString(
      config_provider_, kGcpNoSQLDatabaseClientSpannerDatabaseName,
      client_options_->database_name);
  if (!execution_result.Successful()) {
    SCP_ERROR(kGcpNoSQLDatabaseServiceFactory, kZeroUuid, execution_result,
              "Failed to get Spanner Database name");
    return execution_result;
  }
  return SuccessExecutionResult();
}

std::shared_ptr<NoSQLDatabaseClientProviderInterface>
GcpNoSQLDatabaseServiceFactory::CreateNoSQLDatabaseClient() noexcept {
  return make_shared<GcpNoSQLDatabaseClientProvider>(
      client_options_, instance_client_,
      instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor());
}

}  // namespace google::scp::cpio
