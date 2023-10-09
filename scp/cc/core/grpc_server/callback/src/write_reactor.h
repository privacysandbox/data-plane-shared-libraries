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

#include <memory>
#include <utility>

#include <grpcpp/generic/async_generic_service.h>

#include "core/common/concurrent_queue/src/concurrent_queue.h"
#include "core/common/proto/common.pb.h"
#include "core/interface/errors.h"
#include "core/interface/streaming_context.h"

#include "binary_semaphore.h"

namespace google::scp::core {

/**
 * @brief Helper class to handle server-side streaming. Users should extend this
 * class and implement InitiateCall.
 *
 * @tparam TRequest
 * @tparam TResponse
 */
template <typename TRequest, typename TResponse>
class WriteReactor : public grpc::ServerWriteReactor<TResponse> {
 public:
  explicit WriteReactor(size_t queue_size = 50000) : context_(queue_size) {}

  // Starts the request. Should be called after derived class is done setting
  // up.
  void Start(const TRequest& req) noexcept {
    context_.request = std::make_shared<TRequest>(req);
    // Each time a message is queued, wait until any previous writing is done,
    // then write the next one.
    context_.process_callback = [this](auto& context, bool result_is_valid) {
      // Before trying to StartWrite, we must ensure only one call to StartWrite
      // goes out at one time, because the call is not thread safe. We acquire
      // it at the beginning of the callback because we also guard resp_ since
      // there is only one of it.
      // TODO replace with some try_acquire and requeue the callback on failure.
      sem_.Acquire();
      if (result_is_valid) {
        result_ = context.result;
      }
      resp_ = context.TryGetNextResponse();
      if (resp_ == nullptr) {
        if (!context.IsMarkedDone()) {
          auto dequeue_result = FailureExecutionResult(
              errors::SC_CONCURRENT_QUEUE_CANNOT_DEQUEUE);
          SCP_ERROR_CONTEXT("WriteReactor", context, dequeue_result,
                            "Dequeueing failed but the queue is not done.");
          result_ = dequeue_result;
          context.MarkDone();
        }
        // If we get here, no other elements should be placed on the queue and
        // process_callback will not be called anymore.
        Finalize();
        return;
      }
      if (!result_.Successful()) {
        // Something happened - don't try to write.
        // Release the semaphore for other queued calls to complete.
        sem_.Release();
        return;
      }
      *resp_->mutable_result() = kSuccessResultProto;
      this->StartWrite(resp_.get());
    };
    // Start streaming.
    auto result = InitiateCall(context_);
    if (!result.Successful()) {
      // Handled in context.process_callback.
    }
  }

  // Called after Finish().
  void OnDone() override { delete this; }

  // Each time this is called:
  // write_performed == true: Free up another thread to write the next message.
  // write_performed == false: Some error internal to gRPC occurred.
  void OnWriteDone(bool write_performed) override {
    // Let another thread start writing.
    sem_.Release();
    if (!write_performed) {
      context_.MarkDone();
      this->Finish(
          grpc::Status(grpc::StatusCode::INTERNAL, "Some write failed"));
      return;
    }
  }

  void OnCancel() override {
    // Release the semaphore in case OnCancel gets called and OnWriteDone does
    // not - which would result in a deadlock.
    sem_.Release();
    context_.TryCancel();
  }

 private:
  // Generally, this function is overridden by subclasses to call the method on
  // a class (i.e. client provider) that accepts a ConsumerStreamingContext<>.
  virtual ExecutionResult InitiateCall(
      ConsumerStreamingContext<TRequest, TResponse>) noexcept = 0;

  // Wait for any outstanding writes to finish and then send the final result.
  void Finalize() {
    // Write one more response containing the error and finish.
    resp_ = std::make_unique<TResponse>();
    *resp_->mutable_result() = result_.ToProto();
    if (!result_.Successful()) {
      resp_->mutable_result()->set_error_message(
          errors::GetErrorMessage(resp_->result().status_code()));
    }
    // StartWriteAndFinish does not result it OnWriteDone afterwards.
    this->StartWriteAndFinish(resp_.get(), grpc::WriteOptions(),
                              grpc::Status::OK);
  }

  // Context to pass to InitiateCall.
  ConsumerStreamingContext<TRequest, TResponse> context_;

  const common::proto::ExecutionResult kSuccessResultProto =
      SuccessExecutionResult().ToProto();
  // The result of the async operation.
  ExecutionResult result_ = SuccessExecutionResult();

  BinarySemaphore sem_;
  // The response object to write back to the client.
  std::unique_ptr<TResponse> resp_;
};

}  // namespace google::scp::core
