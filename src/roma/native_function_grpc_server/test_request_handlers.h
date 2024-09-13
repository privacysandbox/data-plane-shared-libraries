/*
 * Copyright 2024 Google LLC
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
#include <string>
#include <utility>

#include <grpcpp/grpcpp.h>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "src/roma/native_function_grpc_server/interface.h"
#include "src/roma/native_function_grpc_server/proto/multi_service.grpc.pb.h"
#include "src/roma/native_function_grpc_server/proto/multi_service.pb.h"
#include "src/roma/native_function_grpc_server/proto/test_service.grpc.pb.h"
#include "src/roma/native_function_grpc_server/proto/test_service.pb.h"

namespace google::scp::roma::grpc_server {
typedef privacy_sandbox::server_common::TestService::AsyncService AsyncService;
typedef privacy_sandbox::multi_service::MultiService::AsyncService
    AsyncMultiService;

template <typename TMetadata>
class TestMethodHandler
    : public RequestHandlerBase<
          privacy_sandbox::server_common::TestMethodRequest,
          privacy_sandbox::server_common::TestMethodResponse, AsyncService> {
 public:
  void Request(TService* service, grpc::ServerContext* ctx,
               grpc::ServerAsyncResponseWriter<TResponse>* responder,
               grpc::ServerCompletionQueue* cq, void* tag) {
    service->RequestTestMethod(ctx, &request_, responder, cq, cq, tag);
  }

  std::pair<TResponse*, grpc::Status> ProcessRequest(
      const TMetadata& metadata) {
    LOG(INFO) << "TestMethod gRPC called.";
    response_.set_output(absl::StrCat(request_.input(), "World. From SERVER"));
    return std::make_pair(&response_, grpc::Status::OK);
  }

 private:
  TRequest request_;
  TResponse response_;
};

template <typename TMetadata>
class TestMethod1Handler
    : public RequestHandlerBase<
          privacy_sandbox::multi_service::TestMethod1Request,
          privacy_sandbox::multi_service::TestMethod1Response,
          AsyncMultiService> {
 public:
  void Request(TService* service, grpc::ServerContext* ctx,
               grpc::ServerAsyncResponseWriter<TResponse>* responder,
               grpc::ServerCompletionQueue* cq, void* tag) override {
    service->RequestTestMethod1(ctx, &request_, responder, cq, cq, tag);
  }

  std::pair<TResponse*, grpc::Status> ProcessRequest(
      const TMetadata& metadata) {
    LOG(INFO) << "TestMethod1 gRPC called.";
    response_.set_output(absl::StrCat(request_.input(), "World. From SERVER"));
    return std::make_pair(&response_, grpc::Status::OK);
  }

 private:
  TRequest request_;
  TResponse response_;
};

template <typename TMetadata>
class TestMethod2Handler
    : public RequestHandlerBase<
          privacy_sandbox::multi_service::TestMethod2Request,
          privacy_sandbox::multi_service::TestMethod2Response,
          AsyncMultiService> {
 public:
  void Request(TService* service, grpc::ServerContext* ctx,
               grpc::ServerAsyncResponseWriter<TResponse>* responder,
               grpc::ServerCompletionQueue* cq, void* tag) override {
    service->RequestTestMethod2(ctx, &request_, responder, cq, cq, tag);
  }

  std::pair<TResponse*, grpc::Status> ProcessRequest(
      const TMetadata& metadata) {
    LOG(INFO) << "TestMethod2 gRPC called.";
    response_.set_output(absl::StrCat(request_.input(), "World. From SERVER"));
    return std::make_pair(&response_, grpc::Status::OK);
  }

 private:
  TRequest request_;
  TResponse response_;
};
}  // namespace google::scp::roma::grpc_server
