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

#include "aws_job_service_factory.h"

#include <memory>

#include "cpio/client_providers/job_client_provider/src/aws/aws_job_client_provider.h"
#include "cpio/client_providers/nosql_database_client_provider/src/aws/aws_nosql_database_client_provider.h"
#include "cpio/client_providers/queue_client_provider/src/aws/aws_queue_client_provider.h"
#include "cpio/server/interface/job_service/configuration_keys.h"
#include "cpio/server/src/instance_service/aws/aws_instance_service_factory.h"
#include "cpio/server/src/service_utils.h"

using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::TryReadConfigString;
using google::scp::cpio::client_providers::AwsJobClientProvider;
using google::scp::cpio::client_providers::AwsNoSQLDatabaseClientProvider;
using google::scp::cpio::client_providers::AwsQueueClientProvider;
using google::scp::cpio::client_providers::JobClientProviderInterface;
using google::scp::cpio::client_providers::NoSQLDatabaseClientProviderInterface;
using google::scp::cpio::client_providers::QueueClientProviderInterface;

namespace google::scp::cpio {
std::shared_ptr<InstanceServiceFactoryInterface>
AwsJobServiceFactory::CreateInstanceServiceFactory() noexcept {
  return std::make_shared<AwsInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

std::shared_ptr<QueueClientProviderInterface>
AwsJobServiceFactory::CreateQueueClient() noexcept {
  return std::make_shared<AwsQueueClientProvider>(
      CreateQueueClientOptions(), instance_client_,
      instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor());
}

std::shared_ptr<NoSQLDatabaseClientProviderInterface>
AwsJobServiceFactory::CreateNoSQLDatabaseClient() noexcept {
  return std::make_shared<AwsNoSQLDatabaseClientProvider>(
      instance_client_, instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor());
}

std::shared_ptr<JobClientProviderInterface>
AwsJobServiceFactory::CreateJobClient() noexcept {
  return std::make_shared<AwsJobClientProvider>(client_options_, queue_client_,
                                                nosql_database_client_);
}
}  // namespace google::scp::cpio
