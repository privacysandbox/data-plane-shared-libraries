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
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "core/async_executor/src/async_executor.h"
#include "core/http2_client/src/http2_client.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/errors.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/service_interface.h"
#include "core/logger/src/logger.h"
#include "core/network/src/grpc_handler.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/public_key_client_provider_interface.h"
#include "cpio/server/interface/public_key_service/configuration_keys.h"
#include "cpio/server/interface/unix_socket_addresses.h"
#include "cpio/server/src/service_utils.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/public_key_service/v1/public_key_service.grpc.pb.h"
#include "public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

using google::cmrt::sdk::public_key_service::v1::ListPublicKeysRequest;
using google::cmrt::sdk::public_key_service::v1::ListPublicKeysResponse;
using google::cmrt::sdk::public_key_service::v1::PublicKeyService;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::HttpClient;
using google::scp::core::HttpClientInterface;
using google::scp::core::ServiceInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::GlobalLogger;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::logger::Logger;
using google::scp::cpio::ExecuteNetworkCall;
using google::scp::cpio::Init;
using google::scp::cpio::kPublicKeyClientCompletionQueueCount;
using google::scp::cpio::kPublicKeyClientIoThreadCount;
using google::scp::cpio::kPublicKeyClientIoThreadPoolQueueCap;
using google::scp::cpio::kPublicKeyClientMaxPollers;
using google::scp::cpio::kPublicKeyClientMinPollers;
using google::scp::cpio::kPublicKeyServiceAddress;
using google::scp::cpio::kPublicKeyVendingServiceEndpoints;
using google::scp::cpio::PublicKeyClientOptions;
using google::scp::cpio::ReadConfigInt;
using google::scp::cpio::ReadConfigStringList;
using google::scp::cpio::Run;
using google::scp::cpio::RunConfigProvider;
using google::scp::cpio::RunLogger;
using google::scp::cpio::RunServer;
using google::scp::cpio::SignalSegmentationHandler;
using google::scp::cpio::Stop;
using google::scp::cpio::StopLogger;
using google::scp::cpio::TryReadConfigInt;
using google::scp::cpio::client_providers::PublicKeyClientProviderFactory;
using google::scp::cpio::client_providers::PublicKeyClientProviderInterface;
using std::bind;
using std::cout;
using std::endl;
using std::list;
using std::make_shared;
using std::make_unique;
using std::map;
using std::move;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using std::placeholders::_1;

namespace {
constexpr int32_t kDefaultNumCompletionQueues = 2;
constexpr int32_t kDefaultMinPollers = 2;
constexpr int32_t kDefaultMaxPollers = 5;
constexpr int kDefaultIoThreadCount = 2;
constexpr int kDefaultIoThreadPoolQueueCap = 100000;

constexpr char kConfigProviderName[] = "config_provider";
constexpr char kAsyncExecutorName[] = "async_executor";
constexpr char kHttpClientName[] = "http_client";
constexpr char kPublicKeyClientName[] = "public_key_client";
}  // namespace

shared_ptr<ConfigProviderInterface> config_provider;
shared_ptr<AsyncExecutorInterface> async_executor;
shared_ptr<HttpClientInterface> http_client;
shared_ptr<PublicKeyClientProviderInterface> public_key_client;

class PublicKeyServiceImpl : public PublicKeyService::CallbackService {
 public:
  grpc::ServerUnaryReactor* ListPublicKeys(
      grpc::CallbackServerContext* server_context,
      const ListPublicKeysRequest* request,
      ListPublicKeysResponse* response) override {
    return ExecuteNetworkCall<ListPublicKeysRequest, ListPublicKeysResponse>(
        server_context, request, response,
        bind(&PublicKeyClientProviderInterface::ListPublicKeys,
             public_key_client, _1));
  }
};

static void SignalHandler(int signum) {
  Stop(public_key_client, kPublicKeyClientName);
  Stop(http_client, kHttpClientName);
  Stop(async_executor, kAsyncExecutorName);
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

  RunClients();

  auto num_completion_queues = kDefaultNumCompletionQueues;
  TryReadConfigInt(config_provider, kPublicKeyClientCompletionQueueCount,
                   num_completion_queues);
  auto min_pollers = kDefaultMinPollers;
  TryReadConfigInt(config_provider, kPublicKeyClientMinPollers, min_pollers);
  auto max_pollers = kDefaultMaxPollers;
  TryReadConfigInt(config_provider, kPublicKeyClientMaxPollers, max_pollers);

  PublicKeyServiceImpl service;
  RunServer<PublicKeyServiceImpl>(service, kPublicKeyServiceAddress,
                                  num_completion_queues, min_pollers,
                                  max_pollers);

  return 0;
}

void RunClients() {
  int32_t io_thread_count;
  if (!TryReadConfigInt(config_provider, kPublicKeyClientIoThreadCount,
                        io_thread_count)
           .Successful()) {
    io_thread_count = kDefaultIoThreadCount;
  }
  int32_t io_thread_pool_queue_cap;
  if (!TryReadConfigInt(config_provider, kPublicKeyClientIoThreadPoolQueueCap,
                        io_thread_pool_queue_cap)
           .Successful()) {
    io_thread_pool_queue_cap = kDefaultIoThreadPoolQueueCap;
  }
  async_executor =
      make_shared<AsyncExecutor>(io_thread_count, io_thread_pool_queue_cap);
  Init(async_executor, kAsyncExecutorName);
  Run(async_executor, kAsyncExecutorName);

  http_client = make_shared<HttpClient>(async_executor);
  list<string> public_key_vending_service_endpoints;
  ReadConfigStringList(config_provider, kPublicKeyVendingServiceEndpoints,
                       public_key_vending_service_endpoints);
  Init(http_client, kHttpClientName);
  Run(http_client, kHttpClientName);

  auto options = make_shared<PublicKeyClientOptions>();
  options->endpoints =
      vector<string>(public_key_vending_service_endpoints.begin(),
                     public_key_vending_service_endpoints.end());
  public_key_client =
      PublicKeyClientProviderFactory::Create(options, http_client);
  Init(public_key_client, kPublicKeyClientName);
  Run(public_key_client, kPublicKeyClientName);
}
