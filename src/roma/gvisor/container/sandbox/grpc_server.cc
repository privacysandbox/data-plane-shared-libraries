// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/roma/gvisor/container/sandbox/grpc_server.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "absl/base/nullability.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/time/time.h"
#include "src/roma/gvisor/container/sandbox/pool_manager.h"
#include "src/roma/gvisor/interface/roma_api.grpc.pb.h"
#include "src/util/status_macro/status_util.h"

namespace privacy_sandbox::server_common::gvisor {

// Logic and data behind the server's behavior.
class RomaGvisorServiceImpl final : public RomaGvisorService::CallbackService {
 public:
  explicit RomaGvisorServiceImpl(
      absl::Nonnull<RomaGvisorPoolManager*> pool_manager)
      : pool_manager_(pool_manager) {}

  grpc::ServerUnaryReactor* ExecuteBinary(
      grpc::CallbackServerContext* context, const ExecuteBinaryRequest* request,
      ExecuteBinaryResponse* response) override {
    absl::StatusOr<absl::Cord> output =
        pool_manager_->SendRequestAndGetResponseFromWorker(
            request->request_id(), request->code_token(),
            request->serialized_request());
    auto* reactor = context->DefaultReactor();
    if (!output.ok()) {
      reactor->Finish(FromAbslStatus(output.status()));
      return reactor;
    }
    std::string output_str;
    absl::CopyCordToString(*std::move(output), &output_str);
    response->set_serialized_response(output_str);
    reactor->Finish(grpc::Status::OK);
    return reactor;
  }

  grpc::ServerUnaryReactor* LoadBinary(grpc::CallbackServerContext* context,
                                       const LoadBinaryRequest* request,
                                       LoadBinaryResponse* response) override {
    auto* reactor = context->DefaultReactor();
    reactor->Finish(
        FromAbslStatus(pool_manager_->LoadBinary(request->code_token())));
    return reactor;
  }

  grpc::Status ExecuteBinaryBidiStreaming(
      grpc::ServerContext* context,
      grpc::ServerReaderWriter<ExecuteBinaryBidiStreamingResponse,
                               ExecuteBinaryBidiStreamingRequest>* stream)
      override {
    LOG(INFO) << "ExecuteBinary Async gRPC called.";

    ExecuteBinaryBidiStreamingRequest request;
    if (bool success = stream->Read(&request); !success) {
      return grpc::Status::CANCELLED;
    }

    absl::StatusOr<absl::Cord> output =
        pool_manager_->SendRequestAndGetResponseFromWorker(
            request.request_id(), request.code_token(),
            request.serialized_request());
    if (!output.status().ok()) {
      return FromAbslStatus(output.status());
    }

    ExecuteBinaryBidiStreamingResponse response;
    std::string output_str;
    absl::CopyCordToString(*std::move(output), &output_str);
    response.set_serialized_response(output_str);

    if (bool success = stream->Write(response); !success) {
      return grpc::Status::CANCELLED;
    }
    return grpc::Status::OK;
  }

 private:
  absl::Nonnull<RomaGvisorPoolManager*> pool_manager_;
};

void RomaGvisorGrpcServer::Run(
    std::string_view server_socket,
    absl::Nonnull<RomaGvisorPoolManager*> pool_manager) {
  RomaGvisorServiceImpl service(pool_manager);

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  grpc::ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  // go/grpc-performance-best-practices#c
  builder.AddListeningPort(absl::StrCat("unix:", server_socket),
                           grpc::InsecureServerCredentials());
  builder.SetMaxSendMessageSize(6'000'000);
  builder.SetMaxReceiveMessageSize(6'000'000);
  builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIME_MS,
                             absl::ToInt64Milliseconds(absl::Minutes(10)));
  builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIMEOUT_MS,
                             absl::ToInt64Milliseconds(absl::Seconds(20)));
  // TODO: ashruti - Fix this. Failed to set zerocopy options on the socket
  // for non-Gvisor
  builder.AddChannelArgument(GRPC_ARG_TCP_TX_ZEROCOPY_ENABLED, 0);
  // Register "service" as the instance through which we'll communicate
  // with clients. In this case it corresponds to an *synchronous*
  // service.
  builder.RegisterService(&service);
  server_ = builder.BuildAndStart();
  server_->GetHealthCheckService()->SetServingStatus(true);

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server_->Wait();
}

}  // namespace privacy_sandbox::server_common::gvisor
