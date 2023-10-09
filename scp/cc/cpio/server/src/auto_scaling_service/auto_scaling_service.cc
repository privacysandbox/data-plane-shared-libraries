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
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <thread>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/errors.h"
#include "core/interface/service_interface.h"
#include "core/logger/src/logger.h"
#include "cpio/client_providers/interface/auto_scaling_client_provider_interface.h"
#include "cpio/client_providers/interface/cloud_initializer_interface.h"
#include "cpio/server/interface/auto_scaling_service/auto_scaling_service_factory_interface.h"
#include "cpio/server/interface/auto_scaling_service/configuration_keys.h"
#include "cpio/server/interface/unix_socket_addresses.h"
#include "cpio/server/src/service_utils.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.grpc.pb.h"
#include "public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.pb.h"

#if defined(AWS_SERVER)
#include "cpio/server/src/auto_scaling_service/aws/aws_auto_scaling_service_factory.h"
#elif defined(TEST_AWS_SERVER)
#include "cpio/server/src/auto_scaling_service/test_aws/test_aws_auto_scaling_service_factory.h"
#else
#error "Must provide [TEST_]AWS_SERVER"
#endif

using google::cmrt::sdk::auto_scaling_service::v1::AutoScalingService;
using google::cmrt::sdk::auto_scaling_service::v1::
    TryFinishInstanceTerminationRequest;
using google::cmrt::sdk::auto_scaling_service::v1::
    TryFinishInstanceTerminationResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ServiceInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::GlobalLogger;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::logger::Logger;
using google::scp::cpio::AutoScalingServiceFactoryInterface;
using google::scp::cpio::ExecuteNetworkCall;
using google::scp::cpio::Init;
using google::scp::cpio::InitializeCloud;
using google::scp::cpio::kAutoScalingClientCompletionQueueCount;
using google::scp::cpio::kAutoScalingClientMaxPollers;
using google::scp::cpio::kAutoScalingClientMinPollers;
using google::scp::cpio::kAutoScalingServiceAddress;
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
using google::scp::cpio::client_providers::AutoScalingClientProviderFactory;
using google::scp::cpio::client_providers::AutoScalingClientProviderInterface;
using google::scp::cpio::client_providers::CloudInitializerInterface;
using std::bind;
using std::cout;
using std::endl;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::placeholders::_1;

namespace {
constexpr int32_t kDefaultNumCompletionQueues = 2;
constexpr int32_t kDefaultMinPollers = 2;
constexpr int32_t kDefaultMaxPollers = 5;

constexpr char kAutoScalingService[] = "AutoScalingService";
constexpr char kCloudInitializerName[] = "cloud_initializer";
constexpr char kConfigProviderName[] = "config_provider";
constexpr char kAutoScalingClientName[] = "auto_scaling_client";
constexpr char kServiceFactoryName[] = "service_factory";
}  // namespace

shared_ptr<CloudInitializerInterface> cloud_initializer;
shared_ptr<ConfigProviderInterface> config_provider;
shared_ptr<AutoScalingClientProviderInterface> auto_scaling_client;
shared_ptr<AutoScalingServiceFactoryInterface> service_factory;

class AutoScalingServiceImpl : public AutoScalingService::CallbackService {
 public:
  grpc::ServerUnaryReactor* TryFinishInstanceTermination(
      grpc::CallbackServerContext* server_context,
      const TryFinishInstanceTerminationRequest* request,
      TryFinishInstanceTerminationResponse* response) override {
    return ExecuteNetworkCall<TryFinishInstanceTerminationRequest,
                              TryFinishInstanceTerminationResponse>(
        server_context, request, response,
        bind(&AutoScalingClientProviderInterface::TryFinishInstanceTermination,
             auto_scaling_client, _1));
  }
};

static void SignalHandler(int signum) {
  Stop(auto_scaling_client, kAutoScalingClientName);
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
  TryReadConfigInt(config_provider, kAutoScalingClientCompletionQueueCount,
                   num_completion_queues);
  auto min_pollers = kDefaultMinPollers;
  TryReadConfigInt(config_provider, kAutoScalingClientMinPollers, min_pollers);
  auto max_pollers = kDefaultMaxPollers;
  TryReadConfigInt(config_provider, kAutoScalingClientMaxPollers, max_pollers);

  AutoScalingServiceImpl service;
  RunServer<AutoScalingServiceImpl>(service, kAutoScalingServiceAddress,
                                    num_completion_queues, min_pollers,
                                    max_pollers);
  return 0;
}

void RunClients() {
#if defined(AWS_SERVER)
  SCP_INFO(kAutoScalingService, kZeroUuid, "Start AWS AutoScaling Server");
  service_factory =
      make_shared<google::scp::cpio::AwsAutoScalingServiceFactory>(
          config_provider);
#elif defined(TEST_AWS_SERVER)
  SCP_INFO(kAutoScalingService, kZeroUuid, "Start test AWS AutoScaling Server");
  service_factory =
      make_shared<google::scp::cpio::TestAwsAutoScalingServiceFactory>(
          config_provider);
#else
#error "Must provide [TEST_]AWS_SERVER"
#endif

  Init(service_factory, kServiceFactoryName);
  Run(service_factory, kServiceFactoryName);

  auto_scaling_client = service_factory->CreateAutoScalingClient();
  Init(auto_scaling_client, kAutoScalingClientName);
  Run(auto_scaling_client, kAutoScalingClientName);
}
