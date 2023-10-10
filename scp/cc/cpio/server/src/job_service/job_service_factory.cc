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

#include "job_service_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "cpio/client_providers/job_client_provider/src/job_client_provider.h"
#include "cpio/server/interface/job_service/configuration_keys.h"
#include "cpio/server/src/service_utils.h"
#include "scp/cc/core/common/uuid/src/uuid.h"

using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::client_providers::NoSQLDatabaseClientOptions;
using google::scp::cpio::client_providers::QueueClientOptions;
using std::make_shared;
using std::shared_ptr;

namespace {
constexpr char kJobServiceFactory[] = "JobServiceFactory";
}  // namespace

namespace google::scp::cpio {

shared_ptr<JobClientOptions>
JobServiceFactory::CreateJobClientOptions() noexcept {
  auto options = make_shared<JobClientOptions>();

  options->job_table_name =
      ReadConfigString(config_provider_, kJobClientJobTableName);
  options->job_queue_name =
      ReadConfigString(config_provider_, kJobClientJobQueueName);

  return options;
}

shared_ptr<InstanceServiceFactoryOptions>
JobServiceFactory::CreateInstanceServiceFactoryOptions() noexcept {
  auto instance_service_factory_options =
      make_shared<InstanceServiceFactoryOptions>();
  instance_service_factory_options
      ->cpu_async_executor_thread_count_config_label = kJobClientCpuThreadCount;
  instance_service_factory_options->cpu_async_executor_queue_cap_config_label =
      kJobClientCpuThreadPoolQueueCap;
  instance_service_factory_options
      ->io_async_executor_thread_count_config_label = kJobClientIoThreadCount;
  instance_service_factory_options->io_async_executor_queue_cap_config_label =
      kJobClientIoThreadPoolQueueCap;
  return instance_service_factory_options;
}

shared_ptr<QueueClientOptions>
JobServiceFactory::CreateQueueClientOptions() noexcept {
  auto queue_options = make_shared<QueueClientOptions>();
  queue_options->queue_name = client_options_->job_queue_name;
  return queue_options;
}

shared_ptr<NoSQLDatabaseClientOptions>
JobServiceFactory::CreateNoSQLDatabaseClientOptions() noexcept {
  auto nosql_database_options = make_shared<NoSQLDatabaseClientOptions>();
  return nosql_database_options;
}

ExecutionResult JobServiceFactory::Init() noexcept {
  client_options_ = CreateJobClientOptions();

  instance_service_factory_options_ = CreateInstanceServiceFactoryOptions();
  instance_service_factory_ = CreateInstanceServiceFactory();

  RETURN_AND_LOG_IF_FAILURE(instance_service_factory_->Init(),
                            kJobServiceFactory, kZeroUuid,
                            "Failed to init InstanceServiceFactory");

  instance_client_ = instance_service_factory_->CreateInstanceClient();
  RETURN_AND_LOG_IF_FAILURE(instance_client_->Init(), kJobServiceFactory,
                            kZeroUuid, "Failed to init InstanceClient");

  queue_client_ = CreateQueueClient();
  RETURN_AND_LOG_IF_FAILURE(queue_client_->Init(), kJobServiceFactory,
                            kZeroUuid, "Failed to init QueueClient");

  nosql_database_client_ = CreateNoSQLDatabaseClient();
  RETURN_AND_LOG_IF_FAILURE(nosql_database_client_->Init(), kJobServiceFactory,
                            kZeroUuid, "Failed to init NoSQLDatabaseClient");

  return SuccessExecutionResult();
}

ExecutionResult JobServiceFactory::Run() noexcept {
  RETURN_AND_LOG_IF_FAILURE(instance_service_factory_->Run(),
                            kJobServiceFactory, kZeroUuid,
                            "Failed to run InstanceServiceFactory");

  RETURN_AND_LOG_IF_FAILURE(instance_client_->Run(), kJobServiceFactory,
                            kZeroUuid, "Failed to run InstanceClient");

  RETURN_AND_LOG_IF_FAILURE(queue_client_->Run(), kJobServiceFactory, kZeroUuid,
                            "Failed to run QueueClient");

  RETURN_AND_LOG_IF_FAILURE(nosql_database_client_->Run(), kJobServiceFactory,
                            kZeroUuid, "Failed to run NoSQLDatabaseClient");

  return SuccessExecutionResult();
}

ExecutionResult JobServiceFactory::Stop() noexcept {
  RETURN_AND_LOG_IF_FAILURE(nosql_database_client_->Stop(), kJobServiceFactory,
                            kZeroUuid, "Failed to stop NoSQLDatabaseClient");

  RETURN_AND_LOG_IF_FAILURE(queue_client_->Stop(), kJobServiceFactory,
                            kZeroUuid, "Failed to stop QueueClient");

  RETURN_AND_LOG_IF_FAILURE(instance_client_->Stop(), kJobServiceFactory,
                            kZeroUuid, "Failed to stop InstanceClient");

  RETURN_AND_LOG_IF_FAILURE(instance_service_factory_->Stop(),
                            kJobServiceFactory, kZeroUuid,
                            "Failed to stop InstanceServiceFactory");

  return SuccessExecutionResult();
}

}  // namespace google::scp::cpio
