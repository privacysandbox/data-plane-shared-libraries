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
#include <string>
#include <thread>

#include "absl/functional/bind_front.h"
#include "core/interface/errors.h"
#include "core/interface/service_interface.h"
#include "core/logger/src/logger.h"
#include "cpio/client_providers/interface/cloud_initializer_interface.h"
#include "cpio/client_providers/interface/parameter_client_provider_interface.h"
#include "cpio/server/interface/parameter_service/configuration_keys.h"
#include "cpio/server/interface/unix_socket_addresses.h"
#include "cpio/server/src/service_utils.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.grpc.pb.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

#if defined(AWS_SERVER)
#include "cpio/server/src/parameter_service/aws/aws_parameter_service_factory.h"
#elif defined(GCP_SERVER)
#include "cpio/server/src/parameter_service/gcp/gcp_parameter_service_factory.h"
#elif defined(TEST_AWS_SERVER)
#include "cpio/server/src/parameter_service/test_aws/test_aws_parameter_service_factory.h"
#elif defined(TEST_GCP_SERVER)
#include "cpio/server/src/parameter_service/test_gcp/test_gcp_parameter_service_factory.h"
#else
#error "Must provide [TEST_]AWS_SERVER or [TEST_]GCP_SERVER"
#endif

using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::cmrt::sdk::parameter_service::v1::ParameterService;
using google::scp::core::AsyncContext;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ServiceInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::GlobalLogger;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::logger::Logger;
using google::scp::cpio::ExecuteNetworkCall;
using google::scp::cpio::Init;
using google::scp::cpio::InitializeCloud;
using google::scp::cpio::kParameterClientCompletionQueueCount;
using google::scp::cpio::kParameterClientMaxPollers;
using google::scp::cpio::kParameterClientMinPollers;
using google::scp::cpio::kParameterServiceAddress;
using google::scp::cpio::ParameterServiceFactoryInterface;
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
using google::scp::cpio::client_providers::CloudInitializerInterface;
using google::scp::cpio::client_providers::ParameterClientProviderFactory;
using google::scp::cpio::client_providers::ParameterClientProviderInterface;

namespace {
constexpr int32_t kDefaultNumCompletionQueues = 2;
constexpr int32_t kDefaultMinPollers = 2;
constexpr int32_t kDefaultMaxPollers = 5;

constexpr char kParameterService[] = "ParameterService";
constexpr char kCloudInitializerName[] = "cloud_initializer";
constexpr char kConfigProviderName[] = "config_provider";
constexpr char kParameterClientName[] = "parameter_client";
constexpr char kServiceFactoryName[] = "service_factory";
}  // namespace

std::shared_ptr<CloudInitializerInterface> cloud_initializer;
std::shared_ptr<ConfigProviderInterface> config_provider;
std::shared_ptr<ParameterClientProviderInterface> parameter_client;
std::shared_ptr<ParameterServiceFactoryInterface> service_factory;

class ParameterServiceImpl : public ParameterService::CallbackService {
 public:
  grpc::ServerUnaryReactor* GetParameter(
      grpc::CallbackServerContext* server_context,
      const GetParameterRequest* request,
      GetParameterResponse* response) override {
    return ExecuteNetworkCall<GetParameterRequest, GetParameterResponse>(
        server_context, request, response,
        absl::bind_front(&ParameterClientProviderInterface::GetParameter,
                         parameter_client));
  }
};

static void SignalHandler(int signum) {
  Stop(parameter_client, kParameterClientName);
  Stop(service_factory, kServiceFactoryName);
  ShutdownCloud(cloud_initializer, kCloudInitializerName);
  StopLogger();
  Stop(config_provider, kConfigProviderName);
  SignalSegmentationHandler(signum);
  exit(signum);
}

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
  TryReadConfigInt(config_provider, kParameterClientCompletionQueueCount,
                   num_completion_queues);
  auto min_pollers = kDefaultMinPollers;
  TryReadConfigInt(config_provider, kParameterClientMinPollers, min_pollers);
  auto max_pollers = kDefaultMaxPollers;
  TryReadConfigInt(config_provider, kParameterClientMaxPollers, max_pollers);

  ParameterServiceImpl service;
  RunServer<ParameterServiceImpl>(service, kParameterServiceAddress,
                                  num_completion_queues, min_pollers,
                                  max_pollers);

  return 0;
}

void RunClients() {
#if defined(AWS_SERVER)
  SCP_INFO(kParameterService, kZeroUuid, "Start AWS Parameter Server");
  service_factory =
      std::make_shared<google::scp::cpio::AwsParameterServiceFactory>(
          config_provider);
#elif defined(GCP_SERVER)
  SCP_INFO(kParameterService, kZeroUuid, "Start GCP Parameter Server");
  service_factory =
      std::make_shared<google::scp::cpio::GcpParameterServiceFactory>(
          config_provider);
#elif defined(TEST_AWS_SERVER)
  SCP_INFO(kParameterService, kZeroUuid, "Start test AWS Parameter Server");
  service_factory =
      std::make_shared<google::scp::cpio::TestAwsParameterServiceFactory>(
          config_provider);
#elif defined(TEST_GCP_SERVER)
  SCP_INFO(kParameterService, kZeroUuid, "Start test GCP Parameter Server");
  service_factory =
      std::make_shared<google::scp::cpio::TestGcpParameterServiceFactory>(
          config_provider);
#endif

  Init(service_factory, kServiceFactoryName);
  Run(service_factory, kServiceFactoryName);

  parameter_client = service_factory->CreateParameterClient();
  Init(parameter_client, kParameterClientName);
  Run(parameter_client, kParameterClientName);
}
