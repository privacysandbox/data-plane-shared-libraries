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

#pragma once

#include <memory>

#include <grpcpp/grpcpp.h>

#include <google/protobuf/message_lite.h>

#include "core/interface/async_context.h"
#include "core/interface/rpc_service_context_interface.h"

#include "grpc_generic_context.h"

namespace google::scp::core {
namespace protobuf = google::protobuf;

/**
 * @brief GrpcHandler. This is the common handler of any gRPC service. The user
 * is expected to provide a callback that takes \a
 * AsyncContext<Request,Response> as input, and in the callback, the response
 * should be produced, and the AsyncContext's \a Finish() function be called.
 * Objects of this class are callable objects and they are designed to be
 * registered as handlers of gRPC method URIs.
 *
 * @tparam TRequest The gRPC service request type.
 * @tparam TResponse The gRPC service response type.
 */
template <typename TRequest, typename TResponse>
class GrpcHandler {
 public:
  static_assert(std::is_base_of_v<protobuf::MessageLite, TRequest>,
                "TRequest must be a valid protobuf message type");
  static_assert(std::is_base_of_v<protobuf::MessageLite, TResponse>,
                "TResponse must be a valid protobuf message type");

  /**
   * @brief Constructs a new Grpc Handler object
   *
   * @param callback The callback which will produce response for given context.
   * @param use_grpc_error_space Whether to return a grpc::UNKNOWN error if
   * callback is unsuccessful. If false, will return a response object with the
   * result inside.
   */
  explicit GrpcHandler(
      typename AsyncContext<TRequest, TResponse>::Callback&& callback,
      bool use_grpc_error_space = true)
      : callback_(callback), use_grpc_error_space_(use_grpc_error_space) {}

  /**
   * @brief The handler function to be called by \a GrpcNetworkService.
   *
   * @param ctx The generic ctx that will provide I/O of the messages.
   */
  void Handle(RPCServiceContextInterface& ctx) {
    // This is the argument to call the supplied callback function.
    AsyncContext<TRequest, TResponse> handler_ctx;
    handler_ctx.request = std::make_shared<TRequest>();
    auto service_ctx = dynamic_cast<GrpcGenericContext*>(&ctx);
    if (service_ctx == nullptr) {
      ctx.Finish();
      return;
    }
    service_ctx->Deserialize<TRequest>(handler_ctx.request);
    handler_ctx.callback = [this,
                            service_ctx](decltype(handler_ctx) handler_ctx) {
      // If the result is non-successful, return an "unknown" error.
      if (!handler_ctx.result && use_grpc_error_space_) {
        service_ctx->SetStatus(
            grpc::Status(grpc::StatusCode::UNKNOWN, "Internal Error"));
        service_ctx->Finish();
        return;
      }
      // Here we must have a valid response. Write it.
      service_ctx->Serialize<TResponse>(handler_ctx.response);
    };
    callback_(handler_ctx);
  }

  /**
   * @brief This operator overload ensures this class can be safely converted
   * into \a RPCServiceContextInterface::Handler.
   */
  inline void operator()(RPCServiceContextInterface& ctx) { this->Handle(ctx); }

 private:
  typename AsyncContext<TRequest, TResponse>::Callback callback_;
  bool use_grpc_error_space_ = true;
};
}  // namespace google::scp::core
