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

#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "core/network/src/grpc_handler.h"
#include "core/network/src/grpc_network_service.h"
#include "core/network/test/helloworld.grpc.pb.h"
#include "core/network/test/helloworld.pb.h"
#include "public/core/test/interface/execution_result_matchers.h"

using helloworld::GreeterService;
using helloworld::SayHelloRequest;
using helloworld::SayHelloResponse;

namespace google::scp::core {
TEST(GRPCServerTest, StartStop) {
  // TODO(https://github.com/grpc/grpc/issues/24730)
  // GRPC cannot bind a random port.
  GrpcNetworkService server(GrpcNetworkService::AddressType::kDNS,
                            "localhost:5555", 10);
  EXPECT_SUCCESS(server.Init());
  EXPECT_SUCCESS(server.Run());
  EXPECT_SUCCESS(server.Stop());
}

TEST(GRPCServerTest, NotRegistered) {
  GrpcNetworkService server(GrpcNetworkService::AddressType::kDNS,
                            "localhost:5555", 2);
  server.Init();
  server.Run();

  auto channel =
      grpc::CreateChannel("localhost:5555", grpc::InsecureChannelCredentials());
  auto stub = GreeterService::NewStub(channel);
  SayHelloRequest request;
  SayHelloResponse reply;
  grpc::ClientContext ctx;
  auto status = stub->SayHello(&ctx, request, &reply);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);

  server.Stop();
}

TEST(GRPCServerTest, SingleCall) {
  GrpcNetworkService server(GrpcNetworkService::AddressType::kDNS,
                            "localhost:5555", 1);
  server.Init();
  server.Run();

  GrpcHandler<SayHelloRequest, SayHelloResponse> handler(
      [](AsyncContext<SayHelloRequest, SayHelloResponse>& ctx) {
        ctx.response = std::make_shared<SayHelloResponse>();
        ctx.response->mutable_message()->append("Foo");
        ctx.result = SuccessExecutionResult();
        ctx.Finish();
      });
  server.RegisterHandler("/helloworld.GreeterService/SayHello", handler);

  auto channel =
      grpc::CreateChannel("localhost:5555", grpc::InsecureChannelCredentials());
  auto stub = GreeterService::NewStub(channel);
  SayHelloRequest request;
  SayHelloResponse reply;
  grpc::ClientContext ctx;
  auto status = stub->SayHello(&ctx, request, &reply);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(reply.message(), "Foo");

  server.Stop();
}

TEST(GRPCServerTest, ConcurrentCalls) {
  GrpcNetworkService server(GrpcNetworkService::AddressType::kDNS,
                            "localhost:5555", 1);
  server.Init();
  server.Run();

  GrpcHandler<SayHelloRequest, SayHelloResponse> handler(
      [](AsyncContext<SayHelloRequest, SayHelloResponse>& ctx) {
        ctx.response = std::make_shared<SayHelloResponse>();
        ctx.response->mutable_message()->append("Foo");
        ctx.result = SuccessExecutionResult();
        ctx.Finish();
      });
  server.RegisterHandler("/helloworld.GreeterService/SayHello", handler);

  auto channel =
      grpc::CreateChannel("localhost:5555", grpc::InsecureChannelCredentials());
  auto stub = GreeterService::NewStub(channel);

  std::vector<std::thread> threads;
  // launch a few threads, each thread does a few calls.
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back(std::thread([&stub]() {
      for (int i = 0; i < 10; ++i) {
        SayHelloRequest request;
        SayHelloResponse reply;
        grpc::ClientContext ctx;
        auto status = stub->SayHello(&ctx, request, &reply);
        EXPECT_TRUE(status.ok());
        EXPECT_EQ(reply.message(), "Foo");
      }
    }));
  }
  for (auto& t : threads) {
    t.join();
  }

  server.Stop();
}

}  // namespace google::scp::core
