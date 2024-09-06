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

#ifndef ROMA_GRPC_SERVER_INTERFACE_H
#define ROMA_GRPC_SERVER_INTERFACE_H

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "absl/log/check.h"
#include "src/roma/metadata_storage/metadata_storage.h"

using google::scp::roma::metadata_storage::MetadataStorage;
using google::scp::roma::metadata_storage::ScopedValueReader;

namespace google::scp::roma::grpc_server {
inline constexpr std::string_view kUuidTag = "request_uuid";

// Alias for type of functions that spawn instances of
// RequestHandlerImpl<TMetadata, THandler to handle rpcs
template <typename TMetadata = std::string>
using FactoryFunction = std::function<void(grpc::ServerCompletionQueue*,
                                           MetadataStorage<TMetadata>*)>;

/**
 * @brief Base class for all handlers to be registered on
 * NativeFunctionGrpcServer. Derived classes should override Request with
 * the gRPC Service method to be invoked, and should override ProcessRequest
 * with custom logic for how the server should handle the gRPC method.
 * Request Type (TRequest), Response Type (TResponse), and Service Type
 * (TService) are provided as aliases to derived classes from this base
 * class.
 *
 * Request object and Response objects should be privately maintained within
 * derived classes as follows:
 *
 * private:
 *   TRequest request_;
 *   TResponse response_;
 */
template <typename TReq, typename TRes, typename TServ>
class RequestHandlerBase {
 public:
  virtual ~RequestHandlerBase() = default;

  using TRequest = TReq;
  using TResponse = TRes;
  using TService = TServ;

  virtual void Request(TService* service, grpc::ServerContext* ctx,
                       grpc::ServerAsyncResponseWriter<TResponse>* responder,
                       grpc::ServerCompletionQueue* cq, void* tag) = 0;

  template <typename TMetadata>
  std::pair<TResponse*, grpc::Status> ProcessRequest(
      const TMetadata& metadata) {
    CHECK(false) << "Derived class must override ProcessRequest";
  }
};

class Proceedable {
 public:
  enum class OpCode { kRun, kShutdown };
  virtual ~Proceedable() = default;
  virtual void Proceed(OpCode opcode) = 0;
};

// Wrapper necessary to safely cast from void*
// Casting from void* to a class with multiple inheritance base class is not
// safe.
class ProceedableWrapper {
 public:
  explicit ProceedableWrapper(Proceedable& proceedable)
      : proceedable_(proceedable) {}
  void Proceed(Proceedable::OpCode opcode) { proceedable_.Proceed(opcode); }

 private:
  Proceedable& proceedable_;
};

// Class encompassing the state and logic needed to serve a request.
template <typename TMetadata, template <typename> class THandler>
class RequestHandlerImpl : public Proceedable, public THandler<TMetadata> {
 public:
  RequestHandlerImpl(typename THandler<TMetadata>::TService* service,
                     grpc::ServerCompletionQueue* cq,
                     MetadataStorage<TMetadata>* ms,
                     FactoryFunction<TMetadata>& factory)
      : service_(service),
        completion_queue_(cq),
        metadata_storage_(ms),
        responder_(&context_),
        status_(State::kCreate),
        factory_(factory),
        this_wrapper_(*this) {
    static_assert(
        std::is_base_of<
            RequestHandlerBase<typename THandler<TMetadata>::TRequest,
                               typename THandler<TMetadata>::TResponse,
                               typename THandler<TMetadata>::TService>,
            THandler<TMetadata>>::value,
        "THandler must be derived from RequestHandlerBase");
    Proceed(Proceedable::OpCode::kRun);
  }

  // States remain as an enum for clarity within subclasses
  enum class State { kCreate, kProcess, kFinish };

  void Proceed(Proceedable::OpCode opcode) override {
    if (opcode == Proceedable::OpCode::kShutdown) {
      status_ = State::kFinish;
    }
    if (status_ == State::kCreate) {
      // Make this instance progress to the PROCESS state.
      status_ = State::kProcess;

      // As part of the initial kCreate state, we *request* that the system
      // start processing requests. In this request, "this"
      // acts are the tag uniquely identifying the request (so that
      // different instances can serve different requests concurrently), in this
      // case the memory address of this RequestHandlerImpl instance.
      this->Request(service_, &context_, &responder_, completion_queue_,
                    &this_wrapper_);
    } else if (status_ == State::kProcess) {
      // Spawn a new instance to serve new clients while we
      // process the one for this RequestHandlerImpl. The instance will
      // deallocate itself as part of its kFinish state.
      factory_(completion_queue_, metadata_storage_);
      typename THandler<TMetadata>::TResponse response;
      grpc::Status status(grpc::StatusCode::NOT_FOUND,
                          "UUID not associated with request");

      auto client_metadata = context_.client_metadata();
      if (auto it = client_metadata.find(std::string(kUuidTag));
          it != client_metadata.end() && metadata_storage_ != nullptr) {
        std::string_view uuid =
            std::string_view(it->second.data(), it->second.size());
        if (auto reader = ScopedValueReader<TMetadata>::Create(
                metadata_storage_->GetMetadataMap(), uuid);
            reader.ok()) {
          if (auto value = reader->Get(); value.ok()) {
            auto response_status_pair = this->ProcessRequest(**value);
            response = *response_status_pair.first;
            status = response_status_pair.second;
          } else {
            status =
                grpc::Status(grpc::StatusCode::NOT_FOUND,
                             "Could not find metadata associated with request");
          }
        } else {
          status = grpc::Status(
              grpc::StatusCode::NOT_FOUND,
              "Could not find mutex for metadata associated with request");
        }
      }

      // And we are done! Let the gRPC runtime know we've finished, using
      // the memory address of this instance as the uniquely identifying
      // tag for the event.
      status_ = State::kFinish;
      responder_.Finish(response, status, &this_wrapper_);
    } else {
      GPR_ASSERT(status_ == State::kFinish);
      // Once in the kFinish state, deallocate ourselves (RequestHandlerImpl).
      delete this;
    }
  }

 protected:
  typename THandler<TMetadata>::TService* service_;
  grpc::ServerCompletionQueue* completion_queue_;
  MetadataStorage<TMetadata>* metadata_storage_;
  grpc::ServerAsyncResponseWriter<typename THandler<TMetadata>::TResponse>
      responder_;
  State status_;
  FactoryFunction<TMetadata> factory_;

  grpc::ServerContext context_;
  ProceedableWrapper this_wrapper_;
};

// Function to handle logic for processing RPCs
template <typename TMetadata>
void HandleRpcs(grpc::ServerCompletionQueue* cq, MetadataStorage<TMetadata>* ms,
                const std::vector<FactoryFunction<TMetadata>>& factories) {
  // Spawn a new RequestHandler instance to serve new clients.
  for (const auto& factory : factories) {
    factory(cq, ms);
  }
  bool ok = true;
  while (ok) {
    // Block waiting to read the next event from the completion queue. The
    // event is uniquely identified by its tag, which in this case is the
    // memory address of a RequestHandler instance.
    // The return value of Next should always be checked. This return value
    // tells us whether there is any kind of event or cq is shutting down.
    void* tag;  // uniquely identifies a request.
    GPR_ASSERT(cq->Next(&tag, &ok));
    ProceedableWrapper* proceedable_wrapper =
        static_cast<ProceedableWrapper*>(tag);
    proceedable_wrapper->Proceed(ok ? Proceedable::OpCode::kRun
                                    : Proceedable::OpCode::kShutdown);
  }
}
}  // namespace google::scp::roma::grpc_server

#endif  // ROMA_GRPC_SERVER_INTERFACE_H
