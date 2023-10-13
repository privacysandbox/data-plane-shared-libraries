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

#include "test_gcp_job_service_factory.h"

#include <memory>

#include "cpio/client_providers/nosql_database_client_provider/src/gcp/gcp_nosql_database_client_provider.h"
#include "cpio/client_providers/nosql_database_client_provider/test/gcp/test_gcp_nosql_database_client_provider.h"
#include "cpio/client_providers/queue_client_provider/test/gcp/test_gcp_queue_client_provider.h"
#include "cpio/server/src/instance_service/test_gcp/test_gcp_instance_service_factory.h"
#include "cpio/server/src/job_service/gcp/gcp_job_service_factory.h"
#include "cpio/server/src/service_utils.h"
#include "public/cpio/test/job_client/test_gcp_job_client_options.h"

#include "test_configuration_keys.h"

using google::scp::cpio::client_providers::GcpNoSQLDatabaseClientProvider;
using google::scp::cpio::client_providers::GcpQueueClientProvider;
using google::scp::cpio::client_providers::NoSQLDatabaseClientOptions;
using google::scp::cpio::client_providers::NoSQLDatabaseClientProviderInterface;
using google::scp::cpio::client_providers::QueueClientOptions;
using google::scp::cpio::client_providers::QueueClientProviderInterface;
using google::scp::cpio::client_providers::TestGcpNoSQLDatabaseClientOptions;
using google::scp::cpio::client_providers::TestGcpQueueClientOptions;
using google::scp::cpio::client_providers::TestGcpQueueClientProvider;
using google::scp::cpio::client_providers::TestGcpSpannerFactory;

namespace google::scp::cpio {
std::shared_ptr<InstanceServiceFactoryInterface>
TestGcpJobServiceFactory::CreateInstanceServiceFactory() noexcept {
  return std::make_shared<TestGcpInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

std::shared_ptr<InstanceServiceFactoryOptions>
TestGcpJobServiceFactory::CreateInstanceServiceFactoryOptions() noexcept {
  auto options = GcpJobServiceFactory::CreateInstanceServiceFactoryOptions();
  auto test_options =
      std::make_shared<TestGcpInstanceServiceFactoryOptions>(*options);
  test_options->project_id_config_label = kTestGcpJobClientProjectId;
  return test_options;
}

std::shared_ptr<JobClientOptions>
TestGcpJobServiceFactory::CreateJobClientOptions() noexcept {
  auto test_options = std::make_shared<TestGcpJobClientOptions>(
      *GcpJobServiceFactory::CreateJobClientOptions());
  TryReadConfigString(config_provider_,
                      kTestGcpJobClientImpersonateServiceAccount,
                      test_options->impersonate_service_account);
  TryReadConfigString(config_provider_, kTestGcpJobClientAccessToken,
                      test_options->access_token);
  return test_options;
}

std::shared_ptr<QueueClientOptions>
TestGcpJobServiceFactory::CreateQueueClientOptions() noexcept {
  auto test_options = std::make_shared<TestGcpQueueClientOptions>(
      *JobServiceFactory::CreateQueueClientOptions());
  test_options->access_token =
      std::dynamic_pointer_cast<TestGcpJobClientOptions>(client_options_)
          ->access_token;
  return test_options;
}

std::shared_ptr<QueueClientProviderInterface>
TestGcpJobServiceFactory::CreateQueueClient() noexcept {
  return std::make_shared<TestGcpQueueClientProvider>(
      std::dynamic_pointer_cast<TestGcpQueueClientOptions>(
          CreateQueueClientOptions()),
      instance_client_, instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor());
}

std::shared_ptr<NoSQLDatabaseClientOptions>
TestGcpJobServiceFactory::CreateNoSQLDatabaseClientOptions() noexcept {
  auto test_options = std::make_shared<TestGcpNoSQLDatabaseClientOptions>(
      *GcpJobServiceFactory::CreateNoSQLDatabaseClientOptions());
  test_options->impersonate_service_account =
      std::dynamic_pointer_cast<TestGcpJobClientOptions>(client_options_)
          ->impersonate_service_account;
  return test_options;
}

std::shared_ptr<NoSQLDatabaseClientProviderInterface>
TestGcpJobServiceFactory::CreateNoSQLDatabaseClient() noexcept {
  return std::make_shared<GcpNoSQLDatabaseClientProvider>(
      CreateNoSQLDatabaseClientOptions(), instance_client_,
      instance_service_factory_->GetCpuAsynceExecutor(),
      instance_service_factory_->GetIoAsynceExecutor(),
      std::make_shared<TestGcpSpannerFactory>());
}

}  // namespace google::scp::cpio
