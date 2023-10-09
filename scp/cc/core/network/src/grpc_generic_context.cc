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

#include "grpc_generic_context.h"

#include <memory>
#include <string>
#include <type_traits>

using std::shared_ptr;

namespace google::scp::core {

GrpcGenericContext::GrpcGenericContext(
    const shared_ptr<::grpc::ServerCompletionQueue>& server_completion_queue,
    const shared_ptr<grpc::AsyncGenericService>& service)
    : completion_queue_(server_completion_queue),
      service_(service),
      server_ctx_(std::make_shared<grpc::GenericServerContext>()),
      server_reader_writer_(server_ctx_.get()),
      grpc_status_(grpc::Status(grpc::StatusCode::UNKNOWN, "Unknown error")),
      state_(State::kRead) {
  Prepare();
}

ExecutionResult GrpcGenericContext::Prepare() noexcept {
  // Prepare for taking a request. This operation essentially async reads a
  // request.
  service_->RequestCall(server_ctx_.get(), &server_reader_writer_,
                        completion_queue_.get(), completion_queue_.get(), this);
  return SuccessExecutionResult();
}

ExecutionResult GrpcGenericContext::Read() {
  server_reader_writer_.Read(&byte_buffer_, this);
  SetState(State::kWrite);
  return SuccessExecutionResult();
}

ExecutionResult GrpcGenericContext::HandleNotFound() {
  grpc_status_ =
      grpc::Status(grpc::StatusCode::NOT_FOUND, "Service or method not found.");
  // Advance the state so that it gets deallocated sooner.
  Finish();
  return SuccessExecutionResult();
}

}  // namespace google::scp::core
