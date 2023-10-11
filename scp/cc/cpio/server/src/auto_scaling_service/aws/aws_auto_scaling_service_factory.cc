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

#include "aws_auto_scaling_service_factory.h"

#include <memory>
#include <utility>

#include "cpio/client_providers/auto_scaling_client_provider/src/aws/aws_auto_scaling_client_provider.h"
#include "cpio/client_providers/interface/auto_scaling_client_provider_interface.h"
#include "cpio/server/interface/auto_scaling_service/auto_scaling_service_factory_interface.h"
#include "cpio/server/interface/auto_scaling_service/configuration_keys.h"
#include "cpio/server/src/instance_service/aws/aws_instance_service_factory.h"
#include "scp/cc/core/common/uuid/src/uuid.h"

using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::client_providers::AutoScalingClientOptions;
using google::scp::cpio::client_providers::AutoScalingClientProviderInterface;
using google::scp::cpio::client_providers::AwsAutoScalingClientProvider;

namespace {
constexpr char kAwsAutoScalingServiceFactory[] = "AwsAutoScalingServiceFactory";
}  // namespace

namespace google::scp::cpio {
std::shared_ptr<InstanceServiceFactoryOptions>
AwsAutoScalingServiceFactory::CreateInstanceServiceFactoryOptions() noexcept {
  auto instance_service_factory_options =
      std::make_shared<InstanceServiceFactoryOptions>();
  instance_service_factory_options
      ->cpu_async_executor_thread_count_config_label =
      kAutoScalingClientCpuThreadCount;
  instance_service_factory_options->cpu_async_executor_queue_cap_config_label =
      kAutoScalingClientCpuThreadPoolQueueCap;
  instance_service_factory_options
      ->io_async_executor_thread_count_config_label =
      kAutoScalingClientIoThreadCount;
  instance_service_factory_options->io_async_executor_queue_cap_config_label =
      kAutoScalingClientIoThreadPoolQueueCap;
  return instance_service_factory_options;
}

std::shared_ptr<InstanceServiceFactoryInterface>
AwsAutoScalingServiceFactory::CreateInstanceServiceFactory() noexcept {
  return std::make_shared<AwsInstanceServiceFactory>(
      config_provider_, instance_service_factory_options_);
}

ExecutionResult AwsAutoScalingServiceFactory::Init() noexcept {
  instance_service_factory_options_ = CreateInstanceServiceFactoryOptions();
  instance_service_factory_ = CreateInstanceServiceFactory();
  auto execution_result = instance_service_factory_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kAwsAutoScalingServiceFactory, kZeroUuid, execution_result,
              "Failed to init InstanceServiceFactory");
    return execution_result;
  }

  instance_client_ = instance_service_factory_->CreateInstanceClient();
  execution_result = instance_client_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kAwsAutoScalingServiceFactory, kZeroUuid, execution_result,
              "Failed to init InstanceClient");
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult AwsAutoScalingServiceFactory::Run() noexcept {
  auto execution_result = instance_service_factory_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kAwsAutoScalingServiceFactory, kZeroUuid, execution_result,
              "Failed to run InstanceServiceFactory");
    return execution_result;
  }

  execution_result = instance_client_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kAwsAutoScalingServiceFactory, kZeroUuid, execution_result,
              "Failed to run InstanceClient");
    return execution_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult AwsAutoScalingServiceFactory::Stop() noexcept {
  auto execution_result = instance_client_->Stop();
  if (!execution_result.Successful()) {
    SCP_ERROR(kAwsAutoScalingServiceFactory, kZeroUuid, execution_result,
              "Failed to stop InstanceClient");
    return execution_result;
  }

  execution_result = instance_service_factory_->Stop();
  if (!execution_result.Successful()) {
    SCP_ERROR(kAwsAutoScalingServiceFactory, kZeroUuid, execution_result,
              "Failed to stop InstanceServiceFactory");
    return execution_result;
  }

  return SuccessExecutionResult();
}

std::shared_ptr<AutoScalingClientProviderInterface>
AwsAutoScalingServiceFactory::CreateAutoScalingClient() noexcept {
  return std::make_shared<AwsAutoScalingClientProvider>(
      std::make_shared<AutoScalingClientOptions>(), instance_client_,
      instance_service_factory_->GetIoAsynceExecutor());
}
}  // namespace google::scp::cpio
