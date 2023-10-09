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

#include "aws_nosql_database_service_factory.h"

#include <memory>
#include <utility>

#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/nosql_database_client_provider_interface.h"
#include "cpio/client_providers/nosql_database_client_provider/src/aws/aws_nosql_database_client_provider.h"
#include "cpio/server/interface/nosql_database_service/configuration_keys.h"
#include "cpio/server/interface/nosql_database_service/nosql_database_service_factory_interface.h"
#include "cpio/server/src/instance_service/aws/aws_instance_service_factory.h"

using google::scp::cpio::client_providers::AwsNoSQLDatabaseClientProvider;
using google::scp::cpio::client_providers::NoSQLDatabaseClientOptions;
using google::scp::cpio::client_providers::NoSQLDatabaseClientProviderInterface;
using std::make_shared;
using std::shared_ptr;

namespace google::scp::cpio {
shared_ptr<InstanceServiceFactoryInterface>
AwsNoSQLDatabaseServiceFactory::CreateInstanceServiceFactory() noexcept {
  return make_shared<AwsInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

std::shared_ptr<NoSQLDatabaseClientProviderInterface>
AwsNoSQLDatabaseServiceFactory::CreateNoSQLDatabaseClient() noexcept {
  return make_shared<AwsNoSQLDatabaseClientProvider>(
      instance_client_, instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor());
}

}  // namespace google::scp::cpio
