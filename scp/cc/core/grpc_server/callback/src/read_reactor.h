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
#include "core/interface/errors.h"
#include "core/interface/streaming_context.h"

namespace google::scp::core {

/**
 * @brief Helper class to handle client-side streaming. Users should extend this
 * class and implement InitiateCall. In the derived class constructor, the last
 * thing done should be "StartRead(&req_)"
 *
 * @tparam TRequest
 * @tparam TResponse
 */
template <typename TRequest, typename TResponse>
class ReadReactor : public grpc::ServerReadReactor<TRequest> {
 public:
  /**
   * @brief Construct a new Read Reactor object
   *
   * @param response A pointer to the response object we should populate when
   * the call is done. We do not own this pointer. Its lifetime should outlive
   * this object.
   * @param queue_size How many request objects should be able to be queued at
   * one time.
   */
  explicit ReadReactor(TResponse* response, size_t queue_size = 50000)
      : resp_(*response), context_(queue_size) {}

  // Called after Finish().
  void OnDone() override { delete this; }

  // Each time this is called:
  // read_performed == true: req_ contains the next streamed message.
  // read_performed == false: Error happened or we reached the end of the
  // stream. Either way no action is necessary.
  void OnReadDone(bool read_performed) override {
    if (!read_performed) {
      context_.MarkDone();
      if (!call_is_initiated_) {
        // Error occurred before reading the 1st message.
        this->Finish(grpc::Status(
            grpc::StatusCode::ABORTED,
            "Some error occurred before reading the first message"));
      }
      // If we called PutBlobStream, then read_performed being false means we
      // are done receiving more messages - marking the queue done above means
      // that InitiateCall is responsible for ensuring Finish is called on the
      // context.
      return;
    }
    if (!call_is_initiated_) {
      SetUpRequest();
      return;
    }
    // Call is already initiated, enqueue this message.
    auto enqueue_result = context_.TryPushRequest(std::move(req_));
    if (!enqueue_result.Successful()) {
      // Client provider has closed the queue, it called context.callback so we
      // can safely end here.
      return;
    }
    this->StartRead(&req_);
  }

  void OnCancel() override { context_.TryCancel(); }

  virtual ~ReadReactor() = default;

 protected:
  TRequest req_;

 private:
  // Generally, this function is overridden by subclasses to call the method on
  // a class (i.e. client provider) that accepts a ProducerStreamingContext<>.
  virtual ExecutionResult InitiateCall(
      ProducerStreamingContext<TRequest, TResponse> context) noexcept = 0;

  void SetUpRequest() noexcept {
    context_.request = std::make_shared<TRequest>(std::move(req_));

    context_.callback = [this](auto& context) mutable {
      result_ = context.result;
      if (result_.Successful()) {
        resp_ = std::move(*context.response);
        *resp_.mutable_result() = result_.ToProto();
      } else {
        // context.response is nullptr in non-success state.
        *resp_.mutable_result() = result_.ToProto();
        resp_.mutable_result()->set_error_message(
            errors::GetErrorMessage(resp_.result().status_code()));
      }
      // Even if error occurred, we don't use gRPC error space to communicate
      // errors of this nature.
      this->Finish(grpc::Status::OK);
    };
    // Start streaming.
    auto result = InitiateCall(context_);
    if (!result.Successful()) {
      // Will be handled in the context_'s callback.
      return;
    }
    call_is_initiated_ = true;
    // Allow reading the next request.
    this->StartRead(&req_);
  }

  // Context to pass to InitiateCall.
  ProducerStreamingContext<TRequest, TResponse> context_;

  // The actual result of the operation, implementation determines the scope.
  ExecutionResult result_ = SuccessExecutionResult();
  // Whether context_ is set up and ready to have more messages pushed onto it.
  bool call_is_initiated_{false};

  // Reference to the response object in which to place the results.
  TResponse& resp_;
};

}  // namespace google::scp::core
