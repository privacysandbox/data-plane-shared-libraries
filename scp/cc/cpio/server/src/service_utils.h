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

#ifndef CPIO_SERVER_SRC_SERVICE_UTILS_H_
#define CPIO_SERVER_SRC_SERVICE_UTILS_H_

#include <functional>
#include <list>
#include <memory>
#include <string>
#include <utility>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

#include "core/interface/async_context.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/errors.h"
#include "core/interface/network_service_interface.h"
#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/cloud_initializer_interface.h"

namespace google::scp::cpio {
namespace internal {
template <typename TRequest, typename TResponse>
void OnExecuteCallback(
    grpc::ServerUnaryReactor* reactor, TResponse* response,
    core::AsyncContext<TRequest, TResponse>& client_context) {
  if (client_context.result.Successful() && client_context.response) {
    *response = std::move(*client_context.response);
  }
  *response->mutable_result() = client_context.result.ToProto();
  if (!client_context.result.Successful()) {
    response->mutable_result()->set_error_message(
        core::errors::GetErrorMessage(client_context.result.status_code));
  }
  reactor->Finish(grpc::Status::OK);
}
}  // namespace internal

// This should be passed to the constructor of GrpcHandler to ensure that if the
// request fails, the error is placed in the ExecutionResult, not in the RPC
// error.
static constexpr bool kGrpcHandlerUseGrpcErrorSpace = false;

template <typename TRequest, typename TResponse>
grpc::ServerUnaryReactor* ExecuteNetworkCall(
    grpc::CallbackServerContext* server_context, const TRequest* request,
    TResponse* response,
    std::function<
        core::ExecutionResult(core::AsyncContext<TRequest, TResponse>&)>
        func) {
  auto* reactor = server_context->DefaultReactor();
  auto activity_id = core::common::Uuid::GenerateUuid();
  core::AsyncContext<TRequest, TResponse> context(
      std::make_shared<TRequest>(*request),
      std::bind(internal::OnExecuteCallback<TRequest, TResponse>, reactor,
                response, std::placeholders::_1),
      activity_id, activity_id);

  // The result returned asynchronously here should be set to context.response
  // and .Finish() is also called at the same time, so we don't need to deal
  // with the result here.
  func(context);

  return reactor;
}

template <typename TService>
void RunServer(TService& service, std::string_view service_address,
               size_t num_completion_queues, size_t min_pollers,
               size_t max_pollers) {
  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  grpc::ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(std::string{service_address},
                           grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Number of completion queues available to handle requests.
  builder.SetSyncServerOption(grpc::ServerBuilder::SyncServerOption::NUM_CQS,
                              num_completion_queues);
  // Minimum number of threads *per completion queue* to listen to incoming
  // requests.
  builder.SetSyncServerOption(
      grpc::ServerBuilder::SyncServerOption::MIN_POLLERS, min_pollers);
  // Maximum number of threads *per completion queue* to listen to incoming
  // requests.
  builder.SetSyncServerOption(
      grpc::ServerBuilder::SyncServerOption::MAX_POLLERS, max_pollers);
  // Timeout for server completion queue's AsyncNext calls.
  builder.SetSyncServerOption(
      grpc::ServerBuilder::SyncServerOption::CQ_TIMEOUT_MSEC, 1000);
  // Finally assemble the server.
  auto server = builder.BuildAndStart();
  std::cout << "Server listening on " << service_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

void Init(const std::shared_ptr<core::ServiceInterface>& service,
          std::string_view service_name);
void Run(const std::shared_ptr<core::ServiceInterface>& service,
         std::string_view service_name);
void Stop(const std::shared_ptr<core::ServiceInterface>& service,
          std::string_view service_name);

void SignalSegmentationHandler(int signum);

void RunLogger(
    const std::shared_ptr<core::ConfigProviderInterface>& config_provider);

void StopLogger();

void InitializeCloud(
    std::shared_ptr<client_providers::CloudInitializerInterface>&
        cloud_initializer,
    std::string_view service_name);

void ShutdownCloud(std::shared_ptr<client_providers::CloudInitializerInterface>&
                       cloud_initializer,
                   std::string_view service_name);

void RunConfigProvider(std::shared_ptr<core::ConfigProviderInterface>&,
                       std::string_view);

void RunNetworkServer(std::shared_ptr<core::NetworkServiceInterface>&, int32_t,
                      std::string_view, std::string_view server_uri);

std::string ReadConfigString(
    const std::shared_ptr<core::ConfigProviderInterface> config_provider,
    std::string_view config_key);

void ReadConfigStringList(
    const std::shared_ptr<core::ConfigProviderInterface> config_provider,
    std::string_view config_key, std::list<std::string>& config_values);

core::ExecutionResult TryReadConfigStringList(
    const std::shared_ptr<core::ConfigProviderInterface> config_provider,
    std::string_view config_key, std::list<std::string>& config_values);

core::ExecutionResult TryReadConfigString(
    const std::shared_ptr<core::ConfigProviderInterface> config_provider,
    std::string_view config_key, std::string& config_value);

core::ExecutionResult TryReadConfigBool(
    const std::shared_ptr<core::ConfigProviderInterface> config_provider,
    std::string_view config_key, bool& config_value);

int32_t ReadConfigInt(
    const std::shared_ptr<core::ConfigProviderInterface> config_provider,
    std::string_view config_key);

core::ExecutionResult TryReadConfigInt(
    const std::shared_ptr<core::ConfigProviderInterface> config_provider,
    std::string_view config_key, int32_t& config_value);

}  // namespace google::scp::cpio

#endif  // CPIO_SERVER_SRC_SERVICE_UTILS_H_
