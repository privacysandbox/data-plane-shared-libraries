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

#include "core/async_executor/src/async_executor.h"
#include "core/http2_client/src/http2_client.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/errors.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/service_interface.h"
#include "core/logger/src/logger.h"
#include "cpio/client_providers/crypto_client_provider/src/crypto_client_provider.h"
#include "cpio/client_providers/interface/crypto_client_provider_interface.h"
#include "cpio/server/interface/crypto_service/configuration_keys.h"
#include "cpio/server/interface/unix_socket_addresses.h"
#include "cpio/server/src/service_utils.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/crypto_client/type_def.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.grpc.pb.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

using google::cmrt::sdk::crypto_service::v1::AeadDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::CryptoService;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeParams;
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
using google::scp::cpio::CryptoClientOptions;
using google::scp::cpio::ExecuteNetworkCall;
using google::scp::cpio::Init;
using google::scp::cpio::kCryptoClientCompletionQueueCount;
using google::scp::cpio::kCryptoClientHpkeAead;
using google::scp::cpio::kCryptoClientHpkeKdf;
using google::scp::cpio::kCryptoClientHpkeKem;
using google::scp::cpio::kCryptoClientMaxPollers;
using google::scp::cpio::kCryptoClientMinPollers;
using google::scp::cpio::kCryptoServiceAddress;
using google::scp::cpio::kHpkeAeadConfigMap;
using google::scp::cpio::kHpkeKdfConfigMap;
using google::scp::cpio::kHpkeKemConfigMap;
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
using google::scp::cpio::TryReadConfigString;
using google::scp::cpio::client_providers::CryptoClientProvider;
using google::scp::cpio::client_providers::CryptoClientProviderInterface;
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

constexpr char kConfigProviderName[] = "config_provider";
constexpr char kCryptoClientName[] = "crypto_client";
}  // namespace

std::shared_ptr<ConfigProviderInterface> config_provider;
std::shared_ptr<CryptoClientProviderInterface> crypto_client;

class CryptoServiceImpl : public CryptoService::CallbackService {
 public:
  grpc::ServerUnaryReactor* HpkeEncrypt(
      grpc::CallbackServerContext* server_context,
      const HpkeEncryptRequest* request,
      HpkeEncryptResponse* response) override {
    return ExecuteNetworkCall<HpkeEncryptRequest, HpkeEncryptResponse>(
        server_context, request, response,
        bind(&CryptoClientProviderInterface::HpkeEncrypt, crypto_client, _1));
  }

  grpc::ServerUnaryReactor* HpkeDecrypt(
      grpc::CallbackServerContext* server_context,
      const HpkeDecryptRequest* request,
      HpkeDecryptResponse* response) override {
    return ExecuteNetworkCall<HpkeDecryptRequest, HpkeDecryptResponse>(
        server_context, request, response,
        bind(&CryptoClientProviderInterface::HpkeDecrypt, crypto_client, _1));
  }

  grpc::ServerUnaryReactor* AeadEncrypt(
      grpc::CallbackServerContext* server_context,
      const AeadEncryptRequest* request,
      AeadEncryptResponse* response) override {
    return ExecuteNetworkCall<AeadEncryptRequest, AeadEncryptResponse>(
        server_context, request, response,
        bind(&CryptoClientProviderInterface::AeadEncrypt, crypto_client, _1));
  }

  grpc::ServerUnaryReactor* AeadDecrypt(
      grpc::CallbackServerContext* server_context,
      const AeadDecryptRequest* request,
      AeadDecryptResponse* response) override {
    return ExecuteNetworkCall<AeadDecryptRequest, AeadDecryptResponse>(
        server_context, request, response,
        bind(&CryptoClientProviderInterface::AeadDecrypt, crypto_client, _1));
  }
};

static void SignalHandler(int signum) {
  Stop(crypto_client, kCryptoClientName);
  StopLogger();
  Stop(config_provider, kConfigProviderName);
  SignalSegmentationHandler(signum);
  exit(signum);
}

void RegisterRpcHandlers();

void RunClients();

int main(int argc, char* argv[]) {
  signal(SIGINT, SignalHandler);
  signal(SIGTERM, SignalHandler);
  signal(SIGPIPE, SIG_IGN);

  RunConfigProvider(config_provider, kConfigProviderName);

  RunLogger(config_provider);

  RunClients();

  auto num_completion_queues = kDefaultNumCompletionQueues;
  TryReadConfigInt(config_provider, kCryptoClientCompletionQueueCount,
                   num_completion_queues);
  auto min_pollers = kDefaultMinPollers;
  TryReadConfigInt(config_provider, kCryptoClientMinPollers, min_pollers);
  auto max_pollers = kDefaultMaxPollers;
  TryReadConfigInt(config_provider, kCryptoClientMaxPollers, max_pollers);

  CryptoServiceImpl service;
  RunServer<CryptoServiceImpl>(service, kCryptoServiceAddress,
                               num_completion_queues, min_pollers, max_pollers);
  return 0;
}

void RunClients() {
  auto options = std::make_shared<CryptoClientOptions>();
  std::string hpke_kem;
  if (TryReadConfigString(config_provider, kCryptoClientHpkeKem, hpke_kem)
          .Successful()) {
    auto it = kHpkeKemConfigMap.find(hpke_kem);
    if (it == kHpkeKemConfigMap.end()) {
      throw runtime_error("Invalid HpkeKem config.");
    }
    options->hpke_params.set_kem(it->second);
  }
  std::string hpke_kdf;
  if (TryReadConfigString(config_provider, kCryptoClientHpkeKdf, hpke_kdf)
          .Successful()) {
    auto it = kHpkeKdfConfigMap.find(hpke_kdf);
    if (it == kHpkeKdfConfigMap.end()) {
      throw runtime_error("Invalid HpkeKdf config.");
    }
    options->hpke_params.set_kdf(it->second);
  }
  std::string hpke_aead;
  if (TryReadConfigString(config_provider, kCryptoClientHpkeAead, hpke_aead)
          .Successful()) {
    auto it = kHpkeAeadConfigMap.find(hpke_aead);
    if (it == kHpkeAeadConfigMap.end()) {
      throw runtime_error("Invalid HpkeAead config.");
    }
    options->hpke_params.set_aead(it->second);
  }

  crypto_client = std::make_shared<CryptoClientProvider>(options);
  Init(crypto_client, kCryptoClientName);
  Run(crypto_client, kCryptoClientName);
}
