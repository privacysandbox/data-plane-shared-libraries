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

#include "core/interface/service_interface.h"
#include "core/logger/src/logger.h"
#include "cpio/client_providers/interface/cloud_initializer_interface.h"
#include "cpio/client_providers/interface/nosql_database_client_provider_interface.h"
#include "cpio/server/interface/nosql_database_service/configuration_keys.h"
#include "cpio/server/interface/nosql_database_service/nosql_database_service_factory_interface.h"
#include "cpio/server/interface/unix_socket_addresses.h"
#include "cpio/server/src/service_utils.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/nosql_database_service/v1/nosql_database_service.grpc.pb.h"
#include "public/cpio/proto/nosql_database_service/v1/nosql_database_service.pb.h"

#if defined(AWS_SERVER)
#include "cpio/server/src/nosql_database_service/aws/aws_nosql_database_service_factory.h"
#elif defined(GCP_SERVER)
#include "cpio/server/src/nosql_database_service/gcp/gcp_nosql_database_service_factory.h"
#elif defined(TEST_AWS_SERVER)
#include "cpio/server/src/nosql_database_service/test_aws/test_aws_nosql_database_service_factory.h"
#elif defined(TEST_GCP_SERVER)
#include "cpio/server/src/nosql_database_service/test_gcp/test_gcp_nosql_database_service_factory.h"
#else
#error "Must provide [TEST_]AWS_SERVER or [TEST_]GCP_SERVER"
#endif

using google::cmrt::sdk::nosql_database_service::v1::CreateDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::CreateDatabaseItemResponse;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemResponse;
using google::cmrt::sdk::nosql_database_service::v1::NoSqlDatabaseService;
using google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::ExecuteNetworkCall;
using google::scp::cpio::Init;
using google::scp::cpio::InitializeCloud;
using google::scp::cpio::kNoSQLDatabaseClientCompletionQueueCount;
using google::scp::cpio::kNoSQLDatabaseClientMaxPollers;
using google::scp::cpio::kNoSQLDatabaseClientMinPollers;
using google::scp::cpio::kNoSqlDatabaseServiceAddress;
using google::scp::cpio::NoSQLDatabaseServiceFactoryInterface;
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
using google::scp::cpio::client_providers::NoSQLDatabaseClientProviderInterface;
using std::bind;
using std::placeholders::_1;

namespace {
constexpr int32_t kDefaultNumCompletionQueues = 2;
constexpr int32_t kDefaultMinPollers = 2;
constexpr int32_t kDefaultMaxPollers = 5;

constexpr char kNoSQLDatabaseServer[] = "NoSQLDatabaseServer";
constexpr char kConfigProviderName[] = "config_provider";
constexpr char kCloudInitializerName[] = "cloud_initializer";
constexpr char kServiceFactoryName[] = "nosql_database_factory";
constexpr char kNoSQLDatabaseClientName[] = "nosql_database_client";
}  // namespace

std::shared_ptr<CloudInitializerInterface> cloud_initializer;
std::shared_ptr<ConfigProviderInterface> config_provider;
std::shared_ptr<NoSQLDatabaseClientProviderInterface> nosql_database_client;
std::shared_ptr<NoSQLDatabaseServiceFactoryInterface> service_factory;

class NoSqlDatabaseServiceImpl : public NoSqlDatabaseService::CallbackService {
 public:
  grpc::ServerUnaryReactor* GetDatabaseItem(
      grpc::CallbackServerContext* server_context,
      const GetDatabaseItemRequest* request,
      GetDatabaseItemResponse* response) override {
    return ExecuteNetworkCall<GetDatabaseItemRequest, GetDatabaseItemResponse>(
        server_context, request, response,
        bind(&NoSQLDatabaseClientProviderInterface::GetDatabaseItem,
             nosql_database_client, _1));
  }

  grpc::ServerUnaryReactor* CreateDatabaseItem(
      grpc::CallbackServerContext* server_context,
      const CreateDatabaseItemRequest* request,
      CreateDatabaseItemResponse* response) override {
    return ExecuteNetworkCall<CreateDatabaseItemRequest,
                              CreateDatabaseItemResponse>(
        server_context, request, response,
        bind(&NoSQLDatabaseClientProviderInterface::CreateDatabaseItem,
             nosql_database_client, _1));
  }

  grpc::ServerUnaryReactor* UpsertDatabaseItem(
      grpc::CallbackServerContext* server_context,
      const UpsertDatabaseItemRequest* request,
      UpsertDatabaseItemResponse* response) override {
    return ExecuteNetworkCall<UpsertDatabaseItemRequest,
                              UpsertDatabaseItemResponse>(
        server_context, request, response,
        bind(&NoSQLDatabaseClientProviderInterface::UpsertDatabaseItem,
             nosql_database_client, _1));
  }
};

static void SignalHandler(int signum) {
  Stop(nosql_database_client, kNoSQLDatabaseClientName);
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
  TryReadConfigInt(config_provider, kNoSQLDatabaseClientCompletionQueueCount,
                   num_completion_queues);
  auto min_pollers = kDefaultMinPollers;
  TryReadConfigInt(config_provider, kNoSQLDatabaseClientMinPollers,
                   min_pollers);
  auto max_pollers = kDefaultMaxPollers;
  TryReadConfigInt(config_provider, kNoSQLDatabaseClientMaxPollers,
                   max_pollers);

  NoSqlDatabaseServiceImpl service;
  RunServer<NoSqlDatabaseServiceImpl>(service, kNoSqlDatabaseServiceAddress,
                                      num_completion_queues, min_pollers,
                                      max_pollers);

  return 0;
}

void RunClients() {
#if defined(AWS_SERVER)
  SCP_INFO(kNoSQLDatabaseServer, kZeroUuid, "Start AWS NoSQLDatabase Server");
  service_factory =
      std::make_shared<google::scp::cpio::AwsNoSQLDatabaseServiceFactory>(
          config_provider);
#elif defined(GCP_SERVER)
  SCP_INFO(kNoSQLDatabaseServer, kZeroUuid, "Start GCP NoSQLDatabase Server");
  service_factory =
      std::make_shared<google::scp::cpio::GcpNoSQLDatabaseServiceFactory>(
          config_provider);
#elif defined(TEST_AWS_SERVER)
  SCP_INFO(kNoSQLDatabaseServer, kZeroUuid,
           "Start test AWS NoSQLDatabase Server");
  service_factory =
      std::make_shared<google::scp::cpio::TestAwsNoSQLDatabaseServiceFactory>(
          config_provider);
#elif defined(TEST_GCP_SERVER)
  SCP_INFO(kNoSQLDatabaseServer, kZeroUuid,
           "Start test GCP NoSQLDatabase Server");
  service_factory =
      std::make_shared<google::scp::cpio::TestGcpNoSQLDatabaseServiceFactory>(
          config_provider);
#endif
  Init(service_factory, kServiceFactoryName);
  Run(service_factory, kServiceFactoryName);

  nosql_database_client = service_factory->CreateNoSQLDatabaseClient();
  Init(nosql_database_client, kNoSQLDatabaseClientName);
  Run(nosql_database_client, kNoSQLDatabaseClientName);
}
