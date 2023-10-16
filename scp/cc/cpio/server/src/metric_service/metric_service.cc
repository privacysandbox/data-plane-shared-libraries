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
#include <iostream>
#include <string>
#include <thread>

#include "absl/functional/bind_front.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/interface/errors.h"
#include "core/interface/service_interface.h"
#include "core/logger/src/logger.h"
#include "cpio/client_providers/interface/cloud_initializer_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "cpio/server/interface/metric_service/configuration_keys.h"
#include "cpio/server/interface/metric_service/metric_service_factory_interface.h"
#include "cpio/server/interface/unix_socket_addresses.h"
#include "cpio/server/src/service_utils.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/metric_client/metric_client_interface.h"
#include "public/cpio/interface/metric_client/type_def.h"
#include "public/cpio/proto/metric_service/v1/metric_service.grpc.pb.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

#if defined(AWS_SERVER)
#include "cpio/server/src/metric_service/aws/aws_metric_service_factory.h"
#elif defined(GCP_SERVER)
#include "cpio/server/src/metric_service/gcp/gcp_metric_service_factory.h"
#elif defined(TEST_AWS_SERVER)
#include "cpio/server/src/metric_service/test_aws/test_aws_metric_service_factory.h"
#elif defined(TEST_GCP_SERVER)
#include "cpio/server/src/metric_service/test_gcp/test_gcp_metric_service_factory.h"
#else
#error "Must provide [TEST_]AWS_SERVER or [TEST_]GCP_SERVER"
#endif

using google::cmrt::sdk::metric_service::v1::MetricService;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
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
using google::scp::cpio::kMetricClientCompletionQueueCount;
using google::scp::cpio::kMetricClientMaxPollers;
using google::scp::cpio::kMetricClientMinPollers;
using google::scp::cpio::kMetricServiceAddress;
using google::scp::cpio::MetricClientInterface;
using google::scp::cpio::MetricClientOptions;
using google::scp::cpio::MetricServiceFactoryInterface;
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
using google::scp::cpio::client_providers::MetricClientProviderFactory;

namespace {
constexpr int32_t kDefaultNumCompletionQueues = 2;
constexpr int32_t kDefaultMinPollers = 2;
constexpr int32_t kDefaultMaxPollers = 5;

constexpr char kMetricService[] = "MetricService";
constexpr char kCloudInitializerName[] = "cloud_initializer";
constexpr char kConfigProviderName[] = "config_provider";
constexpr char kMetricClientName[] = "metric_client";
constexpr char kServiceFactoryName[] = "service_factory";
}  // namespace

std::shared_ptr<CloudInitializerInterface> cloud_initializer;
std::shared_ptr<ConfigProviderInterface> config_provider;
std::shared_ptr<MetricClientInterface> metric_client;
std::shared_ptr<MetricServiceFactoryInterface> service_factory;

class MetricServiceImpl : public MetricService::CallbackService {
 public:
  grpc::ServerUnaryReactor* PutMetrics(
      grpc::CallbackServerContext* server_context,
      const PutMetricsRequest* request, PutMetricsResponse* response) override {
    return ExecuteNetworkCall<PutMetricsRequest, PutMetricsResponse>(
        server_context, request, response,
        absl::bind_front(&MetricClientInterface::PutMetrics, metric_client));
  }
};

static void SignalHandler(int signum) {
  Stop(metric_client, kMetricClientName);
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
  TryReadConfigInt(config_provider, kMetricClientCompletionQueueCount,
                   num_completion_queues);
  auto min_pollers = kDefaultMinPollers;
  TryReadConfigInt(config_provider, kMetricClientMinPollers, min_pollers);
  auto max_pollers = kDefaultMaxPollers;
  TryReadConfigInt(config_provider, kMetricClientMaxPollers, max_pollers);

  MetricServiceImpl service;
  RunServer<MetricServiceImpl>(service, kMetricServiceAddress,
                               num_completion_queues, min_pollers, max_pollers);

  return 0;
}

void RunClients() {
#if defined(AWS_SERVER)
  SCP_INFO(kMetricService, kZeroUuid, "Start AWS Metric Server");
  service_factory =
      std::make_shared<google::scp::cpio::AwsMetricServiceFactory>(
          config_provider);
#elif defined(GCP_SERVER)
  SCP_INFO(kMetricService, kZeroUuid, "Start GCP Metric Server");
  service_factory =
      std::make_shared<google::scp::cpio::GcpMetricServiceFactory>(
          config_provider);
#elif defined(TEST_AWS_SERVER)
  SCP_INFO(kMetricService, kZeroUuid, "Start test AWS Metric Server");
  service_factory =
      std::make_shared<google::scp::cpio::TestAwsMetricServiceFactory>(
          config_provider);
#elif defined(TEST_GCP_SERVER)
  SCP_INFO(kMetricService, kZeroUuid, "Start test GCP Metric Server");
  service_factory =
      std::make_shared<google::scp::cpio::TestGcpMetricServiceFactory>(
          config_provider);
#endif

  Init(service_factory, kServiceFactoryName);
  Run(service_factory, kServiceFactoryName);

  metric_client = service_factory->CreateMetricClient();
  Init(metric_client, kMetricClientName);
  Run(metric_client, kMetricClientName);
}
