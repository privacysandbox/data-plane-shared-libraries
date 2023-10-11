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

#include <execinfo.h>
#include <unistd.h>

#include <csignal>
#include <functional>
#include <string>

#include "core/async_executor/src/async_executor.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/errors.h"
#include "core/interface/service_interface.h"
#include "core/logger/src/logger.h"
#include "cpio/client_providers/interface/cloud_initializer_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/server/interface/instance_service/configuration_keys.h"
#include "cpio/server/interface/instance_service/instance_service_factory_interface.h"
#include "cpio/server/interface/unix_socket_addresses.h"
#include "cpio/server/src/service_utils.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/instance_service/v1/instance_service.grpc.pb.h"
#include "public/cpio/proto/instance_service/v1/instance_service.pb.h"

#if defined(AWS_SERVER)
#include "cpio/server/src/instance_service/aws/aws_instance_service_factory.h"
#elif defined(GCP_SERVER)
#include "cpio/server/src/instance_service/gcp/gcp_instance_service_factory.h"
#elif defined(TEST_AWS_SERVER)
#include "cpio/server/src/instance_service/test_aws/configuration_keys.h"
#include "cpio/server/src/instance_service/test_aws/test_aws_instance_service_factory.h"
#elif defined(TEST_GCP_SERVER)
#include "cpio/server/src/instance_service/test_gcp/configuration_keys.h"
#include "cpio/server/src/instance_service/test_gcp/test_gcp_instance_service_factory.h"
#else
#error "Must provide [TEST_]AWS_SERVER or [TEST_]GCP_SERVER"
#endif

using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::InstanceService;
using google::scp::core::AsyncContext;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ServiceInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::logger::Logger;
using google::scp::cpio::ExecuteNetworkCall;
using google::scp::cpio::Init;
using google::scp::cpio::InitializeCloud;
using google::scp::cpio::InstanceServiceFactoryInterface;
using google::scp::cpio::InstanceServiceFactoryOptions;
using google::scp::cpio::kInstanceClientCompletionQueueCount;
using google::scp::cpio::kInstanceClientCpuThreadCount;
using google::scp::cpio::kInstanceClientCpuThreadPoolQueueCap;
using google::scp::cpio::kInstanceClientIoThreadCount;
using google::scp::cpio::kInstanceClientIoThreadPoolQueueCap;
using google::scp::cpio::kInstanceClientMaxPollers;
using google::scp::cpio::kInstanceClientMinPollers;
using google::scp::cpio::kInstanceServiceAddress;
using google::scp::cpio::ReadConfigInt;
using google::scp::cpio::Run;
using google::scp::cpio::RunConfigProvider;
using google::scp::cpio::RunLogger;
using google::scp::cpio::RunServer;
using google::scp::cpio::ShutdownCloud;
using google::scp::cpio::SignalSegmentationHandler;
using google::scp::cpio::Stop;
using google::scp::cpio::StopLogger;
using google::scp::cpio::TryReadConfigInt;
using google::scp::cpio::TryReadConfigString;
using google::scp::cpio::client_providers::CloudInitializerInterface;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using std::bind;
using std::cout;
using std::endl;
using std::runtime_error;
using std::placeholders::_1;

namespace {
constexpr int32_t kDefaultNumCompletionQueues = 2;
constexpr int32_t kDefaultMinPollers = 2;
constexpr int32_t kDefaultMaxPollers = 5;

constexpr char kInstanceServer[] = "InstanceServer";
constexpr char kCloudInitializerName[] = "cloud_initializer";
constexpr char kConfigProviderName[] = "config_provider";
constexpr char kInstanceClientName[] = "instance_client";
constexpr char kServiceFactoryName[] = "instance_factory";
}  // namespace

std::shared_ptr<CloudInitializerInterface> cloud_initializer;
std::shared_ptr<ConfigProviderInterface> config_provider;
std::shared_ptr<InstanceClientProviderInterface> instance_client;
std::shared_ptr<InstanceServiceFactoryInterface> service_factory;

class InstanceServiceImpl : public InstanceService::CallbackService {
 public:
  grpc::ServerUnaryReactor* GetCurrentInstanceResourceName(
      grpc::CallbackServerContext* server_context,
      const GetCurrentInstanceResourceNameRequest* request,
      GetCurrentInstanceResourceNameResponse* response) override {
    return ExecuteNetworkCall<GetCurrentInstanceResourceNameRequest,
                              GetCurrentInstanceResourceNameResponse>(
        server_context, request, response,
        bind(&InstanceClientProviderInterface::GetCurrentInstanceResourceName,
             instance_client, _1));
  }

  grpc::ServerUnaryReactor* GetTagsByResourceName(
      grpc::CallbackServerContext* server_context,
      const GetTagsByResourceNameRequest* request,
      GetTagsByResourceNameResponse* response) override {
    return ExecuteNetworkCall<GetTagsByResourceNameRequest,
                              GetTagsByResourceNameResponse>(
        server_context, request, response,
        bind(&InstanceClientProviderInterface::GetTagsByResourceName,
             instance_client, _1));
  }

  grpc::ServerUnaryReactor* GetInstanceDetailsByResourceName(
      grpc::CallbackServerContext* server_context,
      const GetInstanceDetailsByResourceNameRequest* request,
      GetInstanceDetailsByResourceNameResponse* response) override {
    return ExecuteNetworkCall<GetInstanceDetailsByResourceNameRequest,
                              GetInstanceDetailsByResourceNameResponse>(
        server_context, request, response,
        bind(&InstanceClientProviderInterface::GetInstanceDetailsByResourceName,
             instance_client, _1));
  }
};

static void SignalHandler(int signum) {
  Stop(instance_client, kInstanceClientName);
  Stop(service_factory, kServiceFactoryName);
  ShutdownCloud(cloud_initializer, kCloudInitializerName);
  StopLogger();
  Stop(config_provider, kConfigProviderName);
  SignalSegmentationHandler(signum);
  exit(signum);
}

void RegisterRpcHandler();

void RunClients();

int main(int argc, char* argv[]) {
  signal(SIGINT, SignalHandler);
  signal(SIGTERM, SignalHandler);
  signal(SIGPIPE, SIG_IGN);

  RunConfigProvider(config_provider, kConfigProviderName);

  RunLogger(config_provider);

  InitializeCloud(cloud_initializer, kCloudInitializerName);

  RunClients();

  auto num_completion_queues = kDefaultNumCompletionQueues;
  TryReadConfigInt(config_provider, kInstanceClientCompletionQueueCount,
                   num_completion_queues);
  auto min_pollers = kDefaultMinPollers;
  TryReadConfigInt(config_provider, kInstanceClientMinPollers, min_pollers);
  auto max_pollers = kDefaultMaxPollers;
  TryReadConfigInt(config_provider, kInstanceClientMaxPollers, max_pollers);

  InstanceServiceImpl service;
  RunServer<InstanceServiceImpl>(service, kInstanceServiceAddress,
                                 num_completion_queues, min_pollers,
                                 max_pollers);

  return 0;
}

void RunClients() {
  auto instance_service_factory_options =
      std::make_shared<InstanceServiceFactoryOptions>();
  instance_service_factory_options
      ->cpu_async_executor_thread_count_config_label =
      kInstanceClientCpuThreadCount;
  instance_service_factory_options->cpu_async_executor_queue_cap_config_label =
      kInstanceClientCpuThreadPoolQueueCap;
  instance_service_factory_options
      ->io_async_executor_thread_count_config_label =
      kInstanceClientIoThreadCount;
  instance_service_factory_options->io_async_executor_queue_cap_config_label =
      kInstanceClientIoThreadPoolQueueCap;

#if defined(AWS_SERVER)
  SCP_INFO(kInstanceServer, kZeroUuid, "Start AWS Instance Server");
  service_factory =
      std::make_shared<google::scp::cpio::AwsInstanceServiceFactory>(
          config_provider, instance_service_factory_options);
#elif defined(GCP_SERVER)
  SCP_INFO(kInstanceServer, kZeroUuid, "Start GCP Instance Server");
  service_factory =
      std::make_shared<google::scp::cpio::GcpInstanceServiceFactory>(
          config_provider, instance_service_factory_options);
#elif defined(TEST_AWS_SERVER)
  SCP_INFO(kInstanceServer, kZeroUuid, "Start test AWS Instance Server");
  auto test_options = std::make_shared<
      google::scp::cpio::TestAwsInstanceServiceFactoryOptions>();
  test_options->region_config_label =
      google::scp::cpio::kTestAwsInstanceClientRegion;
  service_factory =
      std::make_shared<google::scp::cpio::TestAwsInstanceServiceFactory>(
          config_provider, test_options);
#elif defined(TEST_GCP_SERVER)
  SCP_INFO(kInstanceServer, kZeroUuid, "Start test GCP Instance Server");
  auto test_options = std::make_shared<
      google::scp::cpio::TestGcpInstanceServiceFactoryOptions>();
  test_options->project_id_config_label =
      google::scp::cpio::kTestGcpInstanceClientProjectId;
  service_factory =
      std::make_shared<google::scp::cpio::TestGcpInstanceServiceFactory>(
          config_provider, test_options);
#endif

  Init(service_factory, kServiceFactoryName);
  Run(service_factory, kServiceFactoryName);

  instance_client = service_factory->CreateInstanceClient();
  Init(instance_client, kInstanceClientName);
  Run(instance_client, kInstanceClientName);
}
