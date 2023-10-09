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

#pragma once

#include <memory>

#include "core/interface/config_provider_interface.h"
#include "cpio/client_providers/interface/nosql_database_client_provider_interface.h"
#include "cpio/client_providers/interface/queue_client_provider_interface.h"
#include "cpio/server/interface/instance_service/instance_service_factory_interface.h"
#include "cpio/server/interface/job_service/job_service_factory_interface.h"

namespace google::scp::cpio {
/*! @copydoc JobServiceFactoryInterface
 */
class JobServiceFactory : public JobServiceFactoryInterface {
 public:
  explicit JobServiceFactory(
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : config_provider_(config_provider) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

 protected:
  virtual std::shared_ptr<InstanceServiceFactoryOptions>
  CreateInstanceServiceFactoryOptions() noexcept;

  virtual std::shared_ptr<JobClientOptions> CreateJobClientOptions() noexcept;

  virtual std::shared_ptr<client_providers::QueueClientOptions>
  CreateQueueClientOptions() noexcept;

  virtual std::shared_ptr<client_providers::NoSQLDatabaseClientOptions>
  CreateNoSQLDatabaseClientOptions() noexcept;

  virtual std::shared_ptr<InstanceServiceFactoryInterface>
  CreateInstanceServiceFactory() noexcept = 0;

  std::shared_ptr<JobClientOptions> client_options_;

  std::shared_ptr<core::ConfigProviderInterface> config_provider_;

  std::shared_ptr<InstanceServiceFactoryInterface> instance_service_factory_;
  std::shared_ptr<InstanceServiceFactoryOptions>
      instance_service_factory_options_;

  std::shared_ptr<client_providers::InstanceClientProviderInterface>
      instance_client_;

  std::shared_ptr<client_providers::QueueClientProviderInterface> queue_client_;

  std::shared_ptr<client_providers::NoSQLDatabaseClientProviderInterface>
      nosql_database_client_;

 private:
  virtual std::shared_ptr<client_providers::QueueClientProviderInterface>
  CreateQueueClient() noexcept = 0;

  virtual std::shared_ptr<
      client_providers::NoSQLDatabaseClientProviderInterface>
  CreateNoSQLDatabaseClient() noexcept = 0;
};
}  // namespace google::scp::cpio
