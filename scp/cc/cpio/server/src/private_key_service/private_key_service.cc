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
#include <list>
#include <string>
#include <thread>
#include <vector>

#include "core/interface/errors.h"
#include "core/interface/service_interface.h"
#include "core/logger/src/logger.h"
#include "cpio/client_providers/interface/cloud_initializer_interface.h"
#include "cpio/client_providers/interface/private_key_client_provider_interface.h"
#include "cpio/server/interface/private_key_service/configuration_keys.h"
#include "cpio/server/interface/private_key_service/private_key_service_factory_interface.h"
#include "cpio/server/interface/unix_socket_addresses.h"
#include "cpio/server/src/service_utils.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/private_key_service/v1/private_key_service.grpc.pb.h"
#include "public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

#if defined(AWS_SERVER_INSIDE_TEE)
#include "cpio/server/src/private_key_service/aws/tee_aws_private_key_service_factory.h"
#elif defined(AWS_SERVER_OUTSIDE_TEE)
#include "cpio/server/src/private_key_service/aws/nontee_aws_private_key_service_factory.h"
#elif defined(GCP_SERVER_INSIDE_TEE)
#include "cpio/server/src/private_key_service/gcp/gcp_private_key_service_factory.h"
#elif defined(GCP_SERVER_OUTSIDE_TEE)
#include "cpio/server/src/private_key_service/gcp/gcp_private_key_service_factory.h"
#elif defined(TEST_AWS_SERVER)
#include "cpio/server/src/private_key_service/test_aws/test_aws_private_key_service_factory.h"
#elif defined(TEST_GCP_SERVER)
#include "cpio/server/src/private_key_service/gcp/gcp_private_key_service_factory.h"
#else
#error
"Must provide AWS_SERVER_INSIDE_TEE, AWS_SERVER_OUTSIDE_TEE, "
    "GCP_SERVER_INSIDE_TEE, GCP_SERVER_OUTSIDE_TEE, TEST_AWS_SERVER or "
    "TEST_GCP_SERVER"
#endif

using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse;
using google::cmrt::sdk::private_key_service::v1::PrivateKeyService;
using google::scp::core::AsyncContext;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ServiceInterface;
using google::scp::core::common::GlobalLogger;
using google::scp::core::common::kZeroUuid;
using google::scp::core::logger::Logger;
using google::scp::cpio::ExecuteNetworkCall;
using google::scp::cpio::Init;
using google::scp::cpio::InitializeCloud;
using google::scp::cpio::kPrivateKeyClientCompletionQueueCount;
using google::scp::cpio::kPrivateKeyClientMaxPollers;
using google::scp::cpio::kPrivateKeyClientMinPollers;
using google::scp::cpio::kPrivateKeyServiceAddress;
using google::scp::cpio::PrivateKeyServiceFactory;
using google::scp::cpio::PrivateKeyServiceFactoryInterface;
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
using google::scp::cpio::client_providers::PrivateKeyClientProviderInterface;
using std::bind;
using std::cout;
using std::endl;
using std::list;
using std::runtime_error;
using std::placeholders::_1;

namespace {
constexpr int32_t kDefaultNumCompletionQueues = 2;
constexpr int32_t kDefaultMinPollers = 2;
constexpr int32_t kDefaultMaxPollers = 5;

constexpr char kPrivateKeyService[] = "PrivateKeyService";
constexpr char kCloudInitializerName[] = "cloud_initializer";
constexpr char kConfigProviderName[] = "config_provider";
constexpr char kServiceFactoryName[] = "service_factory";
constexpr char kPrivateKeyClientName[] = "private_key_client";
}  // namespace

std::shared_ptr<CloudInitializerInterface> cloud_initializer;
std::shared_ptr<ConfigProviderInterface> config_provider;
std::shared_ptr<PrivateKeyServiceFactoryInterface> service_factory;
std::shared_ptr<PrivateKeyClientProviderInterface> private_key_client;

class PrivateKeyServiceImpl : public PrivateKeyService::CallbackService {
 public:
  grpc::ServerUnaryReactor* ListPrivateKeys(
      grpc::CallbackServerContext* server_context,
      const ListPrivateKeysRequest* request,
      ListPrivateKeysResponse* response) override {
    return ExecuteNetworkCall<ListPrivateKeysRequest, ListPrivateKeysResponse>(
        server_context, request, response,
        bind(&PrivateKeyClientProviderInterface::ListPrivateKeys,
             private_key_client, _1));
  }
};

static void SignalHandler(int signum) {
  Stop(private_key_client, kPrivateKeyClientName);
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
  TryReadConfigInt(config_provider, kPrivateKeyClientCompletionQueueCount,
                   num_completion_queues);
  auto min_pollers = kDefaultMinPollers;
  TryReadConfigInt(config_provider, kPrivateKeyClientMinPollers, min_pollers);
  auto max_pollers = kDefaultMaxPollers;
  TryReadConfigInt(config_provider, kPrivateKeyClientMaxPollers, max_pollers);

  PrivateKeyServiceImpl service;
  RunServer<PrivateKeyServiceImpl>(service, kPrivateKeyServiceAddress,
                                   num_completion_queues, min_pollers,
                                   max_pollers);

  return 0;
}

void RunClients() {
#if defined(AWS_SERVER_INSIDE_TEE)
  SCP_INFO(kPrivateKeyService, kZeroUuid,
           "Start AWS PrivateKey Server inside TEE");
  service_factory =
      std::make_shared<google::scp::cpio::TeeAwsPrivateKeyServiceFactory>(
          config_provider);
#elif defined(AWS_SERVER_OUTSIDE_TEE)
  SCP_INFO(kPrivateKeyService, kZeroUuid,
           "Start AWS PrivateKey Server outside TEE");
  service_factory =
      std::make_shared<google::scp::cpio::NonteePrivateKeyServiceFactory>(
          config_provider);
#elif defined(GCP_SERVER_INSIDE_TEE)
  SCP_INFO(kPrivateKeyService, kZeroUuid,
           "Start GCP PrivateKey Server inside TEE");
  service_factory =
      std::make_shared<google::scp::cpio::GcpPrivateKeyServiceFactory>(
          config_provider);
#elif defined(GCP_SERVER_OUTSIDE_TEE)
  SCP_INFO(kPrivateKeyService, kZeroUuid,
           "Start GCP PrivateKey Server outside TEE");
  service_factory =
      std::make_shared<google::scp::cpio::GcpPrivateKeyServiceFactory>(
          config_provider);
#elif defined(TEST_AWS_SERVER)
  SCP_INFO(kPrivateKeyService, kZeroUuid, "Start test AWS PrivateKey Server");
  service_factory =
      std::make_shared<google::scp::cpio::TestAwsPrivateKeyServiceFactory>(
          config_provider);
#elif defined(TEST_GCP_SERVER)
  SCP_INFO(kPrivateKeyService, kZeroUuid, "Start test GCP PrivateKey Server");
  service_factory =
      std::make_shared<google::scp::cpio::GcpPrivateKeyServiceFactory>(
          config_provider);
#endif

  Init(service_factory, kServiceFactoryName);
  Run(service_factory, kServiceFactoryName);

  private_key_client = service_factory->CreatePrivateKeyClient();
  Init(private_key_client, kPrivateKeyClientName);
  Run(private_key_client, kPrivateKeyClientName);
}
