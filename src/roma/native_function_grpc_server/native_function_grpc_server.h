/*
 * Copyright 2023 Google LLC
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

#ifndef ROMA_GRPC_SERVER_NATIVE_FUNCTION_GRPC_SERVER_H_
#define ROMA_GRPC_SERVER_NATIVE_FUNCTION_GRPC_SERVER_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "src/core/common/uuid/uuid.h"
#include "src/roma/native_function_grpc_server/interface.h"
#include "src/roma/native_function_grpc_server/proto/logging_service.grpc.pb.h"
#include "src/roma/native_function_grpc_server/proto/logging_service.pb.h"
#include "src/roma/sandbox/native_function_binding/thread_safe_map.h"

namespace google::scp::roma::grpc_server {
typedef privacy_sandbox::server_common::LoggingService::AsyncService
    AsyncLoggingService;
using roma::sandbox::native_function_binding::ThreadSafeMap;

/**
 * @brief Implementation of Request Handler for Logging.
 */
template <typename TMetadata>
class LogImpl
    : public RequestHandlerBase<privacy_sandbox::server_common::LogRequest,
                                privacy_sandbox::server_common::LogResponse,
                                AsyncLoggingService> {
 public:
  void Request(TService* service, grpc::ServerContext* ctx,
               grpc::ServerAsyncResponseWriter<TResponse>* responder,
               grpc::ServerCompletionQueue* cq, void* tag) override {
    service->RequestLog(ctx, &request_, responder, cq, cq, tag);
  }

  std::pair<TResponse*, grpc::Status> ProcessRequest(
      const TMetadata& metadata) {
    LOG(INFO) << "Log gRPC called.";
    LOG(LEVEL(ToSeverity(request_.severity()))) << request_.input();
    return std::make_pair(&response_, grpc::Status::OK);
  }

 private:
  absl::LogSeverity ToSeverity(
      privacy_sandbox::server_common::Severity severity) {
    switch (severity) {
      case privacy_sandbox::server_common::Severity::SEVERITY_ERROR:
        return absl::LogSeverity::kError;
      case privacy_sandbox::server_common::Severity::SEVERITY_WARNING:
        return absl::LogSeverity::kWarning;
      case privacy_sandbox::server_common::Severity::SEVERITY_INFO:
        return absl::LogSeverity::kInfo;
    }
    LOG(ERROR) << "Unexpected value for Severity: "
               << static_cast<int>(severity);
    return absl::LogSeverity::kFatal;
  }

  TRequest request_;
  TResponse response_;
};

template <typename TMetadata = std::string>
class NativeFunctionGrpcServer final {
 public:
  NativeFunctionGrpcServer() = delete;

  explicit NativeFunctionGrpcServer(
      const std::vector<std::string>& server_addresses) {
    for (const auto& addr : server_addresses) {
      builder_.AddListeningPort(addr, grpc::InsecureServerCredentials());
    }
    AddService(std::make_unique<AsyncLoggingService>(), LogImpl<TMetadata>());
  }

  ~NativeFunctionGrpcServer() {
    // Always shutdown the completion queue after the server.
    completion_queue_->Shutdown();
    DrainQueue(completion_queue_.get());
  }

  void Shutdown() { server_->Shutdown(); }

  /** @brief Adds a gRPC service and handler classes for all associated rpc
   * methods to server for invocation of RPCs from arbitrary services.
   */
  template <template <typename> typename... THandlers>
  void AddService(std::unique_ptr<grpc::Service> service,
                  THandlers<TMetadata>&&... handlers) {
    // Store service ptr to ensure service exists for the lifetime of the
    // server instance returned by BuildAndStart(). Handler stored to be able to
    // listen for async RPCs when TestAsyncServer is run.
    services_.push_back(std::move(service));

    (CreateFactory(handlers), ...);

    builder_.RegisterService(services_.back().get());
  }

  absl::Status StoreMetadata(std::string uuid, TMetadata metadata) {
    return metadata_map_.Add(std::move(uuid), std::move(metadata));
  }

  void DeleteMetadata(std::string_view uuid) { metadata_map_.Delete(uuid); }

  void Run() {
    // Start accepting requests to the server.
    completion_queue_ = builder_.AddCompletionQueue();
    server_ = builder_.BuildAndStart();
    HandleRpcs<TMetadata>(completion_queue_.get(), &metadata_map_, factories_);
  }

 private:
  void DrainQueue(grpc::CompletionQueue* cq) {
    void* tag;
    bool ignored_ok;
    while (cq->Next(&tag, &ignored_ok)) {
      ProceedableWrapper* proceedable_wrapper =
          static_cast<ProceedableWrapper*>(tag);
      proceedable_wrapper->Proceed(Proceedable::OpCode::kShutdown);
    }
  }

  /** @brief Creates a new factory function for generating
   * RequestHandlerImpl<TMetadata, THandler> instances.
   */
  template <template <typename> typename THandler>
  void CreateFactory(THandler<TMetadata>) {
    const size_t index = factories_.size();
    factories_.push_back([service_ptr = services_.back().get(), index, this]() {
      new RequestHandlerImpl<TMetadata, THandler>(
          static_cast<typename THandler<TMetadata>::TService*>(service_ptr),
          completion_queue_.get(), &metadata_map_, factories_[index]);
    });
  }

  std::unique_ptr<grpc::ServerCompletionQueue> completion_queue_;
  ThreadSafeMap<TMetadata> metadata_map_;
  std::vector<std::unique_ptr<grpc::Service>> services_;
  std::vector<std::function<void()>> factories_;
  std::unique_ptr<grpc::Server> server_;
  grpc::ServerBuilder builder_;
};
}  // namespace google::scp::roma::grpc_server

#endif  // ROMA_GRPC_SERVER_NATIVE_FUNCTION_GRPC_SERVER_H_
