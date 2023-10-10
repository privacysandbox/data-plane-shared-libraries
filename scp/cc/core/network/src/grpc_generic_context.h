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

#ifndef CORE_NETWORK_SRC_GRPC_GENERIC_CONTEXT_H_
#define CORE_NETWORK_SRC_GRPC_GENERIC_CONTEXT_H_

#include <memory>
#include <string>
#include <type_traits>

#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/proto_utils.h>

#include <google/protobuf/message_lite.h>

#include "core/interface/async_context.h"
#include "core/interface/rpc_service_context_interface.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::core {
/**
 * @brief The gRPC processing context.
 *
 * This class is used as "tag" of the gRPC server's \a CompletionQueues. The
 * lifetime of each object is tied to each RPC request processing. To read a
 * request, \a Prepare() is called to notify the server that a request is
 * expected; A request is ready (or error happened) when it comes out of the \a
 * CompletionQueue::Next() as tag. After \a Finish() is called, it will come out
 * of \a CompletionQueue::Next() again for final destruction, concluding the
 * request handling.
 *
 */
class GrpcGenericContext : public RPCServiceContextInterface {
 public:
  /// The processing state machine required by gRPC's async server processing
  /// model.
  enum class State { kRead = 0, kWrite = 1, kFinish = 2, kDestroy = 3 };

  /**
   * @brief Constructs a new GrpcServiceContext object
   *
   * @param cq  The completion queue this context is associated to.
   * @param service The async service registered into the network service.
   */
  GrpcGenericContext(const std::shared_ptr<grpc::ServerCompletionQueue>&
                         server_completion_queue,
                     const std::shared_ptr<grpc::AsyncGenericService>& service);

  ExecutionResult Prepare() noexcept override;

  ExecutionResult Process() noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult Finish() noexcept override {
    server_reader_writer_.Finish(grpc_status_, this);
    state_ = State::kDestroy;
    return SuccessExecutionResult();
  }

  /// Perform read from the remote into our internal buffer.
  ExecutionResult Read();

  /// To be called by the server when a request URI is not registered.
  ExecutionResult HandleNotFound();

  /// Get the method URI of the request.
  const std::string& Method() { return server_ctx_->method(); }

  /// Get the internal state.
  State GetState() { return state_; }

  /// Set the internal state.
  void SetState(State state) { state_ = state; }

  /// Set the gRPC processing status.
  void SetStatus(const grpc::Status& status) { grpc_status_ = status; }

  /// Deserialize from the internal buffer to \p message.
  template <class T>
  ExecutionResult Deserialize(
      const std::shared_ptr<google::protobuf::MessageLite>& message) {
    auto status = grpc::GenericDeserialize<grpc::ProtoBufferReader, T>(
        &byte_buffer_, message.get());
    if (status.ok()) {
      return SuccessExecutionResult();
    }
    grpc_status_ = status;
    return FailureExecutionResult(errors::SC_NETWORK_SERVICE_CANNOT_PARSE);
  }

  /// Serialize \p message to internal buffer and send it.
  template <class T>
  ExecutionResult Serialize(
      const std::shared_ptr<google::protobuf::MessageLite>& message) {
    bool own_buffer;
    auto status = grpc::GenericSerialize<grpc::ProtoBufferWriter, T>(
        *message, &byte_buffer_, &own_buffer);
    if (!status.ok()) {
      grpc_status_ = status;
      return FailureExecutionResult(
          errors::SC_NETWORK_SERVICE_CANNOT_SERIALIZE);
    }
    server_reader_writer_.Write(byte_buffer_, this);
    grpc_status_ = grpc::Status::OK;
    return SuccessExecutionResult();
  }

  size_t visit_cnt_ = 0;

 private:
  /// The CompletionQueue this context is associated with. See
  /// https://grpc.github.io/grpc/cpp/classgrpc_1_1_completion_queue.html
  std::shared_ptr<grpc::ServerCompletionQueue> completion_queue_;
  /// The service associated with the server and this context. The service is
  /// responsible for initiating new request handlings.
  std::shared_ptr<grpc::AsyncGenericService> service_;
  /// The context object that is required by gRPC to pass along with the
  /// context. We use it to figure out the request URI.
  std::shared_ptr<grpc::GenericServerContext> server_ctx_;
  /// The gRPC high-level io interface provided to server. We use it to read and
  /// write messages.
  grpc::GenericServerAsyncReaderWriter server_reader_writer_;
  /// The internal buffer to interact with \a server_reader_writer_.
  grpc::ByteBuffer byte_buffer_;
  /// The gRPC status to return to the client.
  grpc::Status grpc_status_;
  /// The internal state machine. This is required by gRPC async server
  /// semantics.
  State state_;
};

}  // namespace google::scp::core

#endif  // CORE_NETWORK_SRC_GRPC_GENERIC_CONTEXT_H_
