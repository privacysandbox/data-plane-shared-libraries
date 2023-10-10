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

#include <condition_variable>
#include <csignal>
#include <functional>
#include <string>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/grpcpp.h>

#include "core/common/uuid/src/uuid.h"
#include "core/grpc_server/callback/src/read_reactor.h"
#include "core/grpc_server/callback/src/write_reactor.h"
#include "core/interface/errors.h"
#include "core/logger/src/logger.h"
#include "cpio/client_providers/interface/blob_storage_client_provider_interface.h"
#include "cpio/client_providers/interface/cloud_initializer_interface.h"
#include "cpio/server/interface/blob_storage_service/blob_storage_service_factory_interface.h"
#include "cpio/server/interface/blob_storage_service/configuration_keys.h"
#include "cpio/server/interface/unix_socket_addresses.h"
#include "cpio/server/src/service_utils.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/blob_storage_service/v1/blob_storage_service.grpc.pb.h"
#include "public/cpio/proto/blob_storage_service/v1/blob_storage_service.pb.h"
#if defined(AWS_SERVER)
#include "cpio/server/src/blob_storage_service/aws/aws_blob_storage_service_factory.h"
#elif defined(GCP_SERVER)
#include "cpio/server/src/blob_storage_service/gcp/gcp_blob_storage_service_factory.h"
#elif defined(TEST_AWS_SERVER)
#include "cpio/server/src/blob_storage_service/test_aws/test_aws_blob_storage_service_factory.h"
#elif defined(TEST_GCP_SERVER)
#include "cpio/server/src/blob_storage_service/test_gcp/test_gcp_blob_storage_service_factory.h"
#else
#error "Must provide [TEST_]AWS_SERVER or [TEST_]GCP_SERVER"
#endif

using google::cmrt::sdk::blob_storage_service::v1::BlobStorageService;
using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ConsumerStreamingContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ProducerStreamingContext;
using google::scp::core::ReadReactor;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::WriteReactor;
using google::scp::core::common::ConcurrentQueue;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetErrorMessage;
using google::scp::cpio::BlobStorageServiceFactoryInterface;
using google::scp::cpio::ExecuteNetworkCall;
using google::scp::cpio::Init;
using google::scp::cpio::InitializeCloud;
using google::scp::cpio::kBlobStorageClientCompletionQueueCount;
using google::scp::cpio::kBlobStorageClientMaxPollers;
using google::scp::cpio::kBlobStorageClientMinPollers;
using google::scp::cpio::kBlobStorageServiceAddress;
using google::scp::cpio::Run;
using google::scp::cpio::RunConfigProvider;
using google::scp::cpio::RunLogger;
using google::scp::cpio::RunServer;
using google::scp::cpio::ShutdownCloud;
using google::scp::cpio::SignalSegmentationHandler;
using google::scp::cpio::Stop;
using google::scp::cpio::StopLogger;
using google::scp::cpio::TryReadConfigInt;
using google::scp::cpio::client_providers::BlobStorageClientProviderInterface;
using google::scp::cpio::client_providers::CloudInitializerInterface;
using grpc::ServerContext;
using grpc::ServerReadReactor;
using grpc::ServerWriteReactor;
using std::bind;
using std::make_shared;
using std::shared_ptr;
using SyncServerOption = grpc::ServerBuilder::SyncServerOption;
using std::placeholders::_1;

namespace {
constexpr char kServiceFactoryName[] = "blob_storage_factory";
constexpr char kBlobStorageServer[] = "blob_storage_server";
constexpr char kConfigProviderName[] = "config_provider";
constexpr char kCloudInitializerName[] = "cloud_initializer";
constexpr char kBlobStorageClientName[] = "blob_storage_client";

constexpr int32_t kDefaultNumCompletionQueues = 2;
constexpr int32_t kDefaultMinPollers = 2;
constexpr int32_t kDefaultMaxPollers = 5;

shared_ptr<CloudInitializerInterface> cloud_initializer;
shared_ptr<ConfigProviderInterface> config_provider;
shared_ptr<BlobStorageClientProviderInterface> blob_storage_client;
shared_ptr<BlobStorageServiceFactoryInterface> service_factory;

void SignalHandler(int signum) {
  Stop(blob_storage_client, kBlobStorageClientName);
  Stop(service_factory, kServiceFactoryName);
  ShutdownCloud(cloud_initializer, kCloudInitializerName);
  StopLogger();
  Stop(config_provider, kConfigProviderName);
  SignalSegmentationHandler(signum);
  exit(signum);
}

}  // namespace

void RunClients();

class BlobStreamReadReactor
    : public ReadReactor<PutBlobStreamRequest, PutBlobStreamResponse> {
 public:
  explicit BlobStreamReadReactor(PutBlobStreamResponse* resp)
      : ReadReactor(resp) {
    this->StartRead(&req_);
  }

 private:
  ExecutionResult InitiateCall(
      ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>
          context) noexcept override {
    return blob_storage_client->PutBlobStream(context);
  }
};

class BlobStreamWriteReactor
    : public WriteReactor<GetBlobStreamRequest, GetBlobStreamResponse> {
 public:
  explicit BlobStreamWriteReactor(const GetBlobStreamRequest& req) {
    this->Start(req);
  }

 private:
  ExecutionResult InitiateCall(
      ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse>
          context) noexcept override {
    return blob_storage_client->GetBlobStream(context);
  }
};

class BlobStorageServiceImpl : public BlobStorageService::CallbackService {
 public:
  ServerReadReactor<PutBlobStreamRequest>* PutBlobStream(
      grpc::CallbackServerContext* server_context,
      PutBlobStreamResponse* response) override {
    return new BlobStreamReadReactor(response);
  }

  ServerWriteReactor<GetBlobStreamResponse>* GetBlobStream(
      grpc::CallbackServerContext* server_context,
      const GetBlobStreamRequest* request) override {
    return new BlobStreamWriteReactor(*request);
  }

  grpc::ServerUnaryReactor* GetBlob(grpc::CallbackServerContext* server_context,
                                    const GetBlobRequest* request,
                                    GetBlobResponse* response) override {
    return ExecuteNetworkCall<GetBlobRequest, GetBlobResponse>(
        server_context, request, response,
        bind(&BlobStorageClientProviderInterface::GetBlob, blob_storage_client,
             _1));
  }

  grpc::ServerUnaryReactor* PutBlob(grpc::CallbackServerContext* server_context,
                                    const PutBlobRequest* request,
                                    PutBlobResponse* response) override {
    return ExecuteNetworkCall<PutBlobRequest, PutBlobResponse>(
        server_context, request, response,
        bind(&BlobStorageClientProviderInterface::PutBlob, blob_storage_client,
             _1));
  }

  grpc::ServerUnaryReactor* ListBlobsMetadata(
      grpc::CallbackServerContext* server_context,
      const ListBlobsMetadataRequest* request,
      ListBlobsMetadataResponse* response) override {
    return ExecuteNetworkCall<ListBlobsMetadataRequest,
                              ListBlobsMetadataResponse>(
        server_context, request, response,
        bind(&BlobStorageClientProviderInterface::ListBlobsMetadata,
             blob_storage_client, _1));
  }

  grpc::ServerUnaryReactor* DeleteBlob(
      grpc::CallbackServerContext* server_context,
      const DeleteBlobRequest* request, DeleteBlobResponse* response) override {
    return ExecuteNetworkCall<DeleteBlobRequest, DeleteBlobResponse>(
        server_context, request, response,
        bind(&BlobStorageClientProviderInterface::DeleteBlob,
             blob_storage_client, _1));
  }
};

int main(int argc, char* argv[]) {
  signal(SIGINT, SignalHandler);
  signal(SIGTERM, SignalHandler);
  signal(SIGPIPE, SIG_IGN);

  RunConfigProvider(config_provider, kConfigProviderName);

  RunLogger(config_provider);

  InitializeCloud(cloud_initializer, kCloudInitializerName);

  RunClients();

  auto num_completion_queues = kDefaultNumCompletionQueues;
  TryReadConfigInt(config_provider, kBlobStorageClientCompletionQueueCount,
                   num_completion_queues);
  auto min_pollers = kDefaultMinPollers;
  TryReadConfigInt(config_provider, kBlobStorageClientMinPollers, min_pollers);
  auto max_pollers = kDefaultMaxPollers;
  TryReadConfigInt(config_provider, kBlobStorageClientMaxPollers, max_pollers);

  BlobStorageServiceImpl service;
  RunServer<BlobStorageServiceImpl>(service, kBlobStorageServiceAddress,
                                    num_completion_queues, min_pollers,
                                    max_pollers);

  return 0;
}

void RunClients() {
#if defined(AWS_SERVER)
  SCP_INFO(kBlobStorageServer, kZeroUuid, "Start AWS BlobStorage Server");
  service_factory =
      make_shared<google::scp::cpio::AwsBlobStorageServiceFactory>(
          config_provider);
#elif defined(GCP_SERVER)
  SCP_INFO(kBlobStorageServer, kZeroUuid, "Start GCP BlobStorage Server");
  service_factory =
      make_shared<google::scp::cpio::GcpBlobStorageServiceFactory>(
          config_provider);
#elif defined(TEST_AWS_SERVER)
  SCP_INFO(kBlobStorageServer, kZeroUuid, "Start test AWS BlobStorage Server");
  service_factory =
      make_shared<google::scp::cpio::TestAwsBlobStorageServiceFactory>(
          config_provider);
#elif defined(TEST_GCP_SERVER)
  SCP_INFO(kBlobStorageServer, kZeroUuid, "Start test GCP BlobStorage Server");
  service_factory =
      make_shared<google::scp::cpio::TestGcpBlobStorageServiceFactory>(
          config_provider);
#endif
  Init(service_factory, kServiceFactoryName);
  Run(service_factory, kServiceFactoryName);

  blob_storage_client = service_factory->CreateBlobStorageClient();
  Init(blob_storage_client, kBlobStorageClientName);
  Run(blob_storage_client, kBlobStorageClientName);
}
