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

#include "grpc_network_service.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"

#include "error_codes.h"
#include "grpc_generic_context.h"

using google::scp::core::common::kZeroUuid;
using grpc::AsyncGenericService;
using grpc::InsecureServerCredentials;
using grpc::ResourceQuota;
using grpc::ServerBuilder;

using std::bad_alloc;
using std::condition_variable;
using std::make_shared;
using std::mutex;
using std::ref;
using std::scoped_lock;
using std::string;
using std::thread;
using std::unique_lock;

static constexpr char kGrpcNetworkService[] = "GrpcNetworkService";

namespace google::scp::core {
ExecutionResult GrpcNetworkService::Init() noexcept {
  // If this has been initialized already, or somehow builder_ is valid,
  // return error.
  if (server_builder_ || service_) {
    auto execution_result =
        FailureExecutionResult(errors::SC_NETWORK_SERVICE_DOUBLE_INIT_ERROR);
    SCP_ERROR(kGrpcNetworkService, kZeroUuid, execution_result,
              "Network is double initialized");
    return execution_result;
  }
  try {
    service_ = make_shared<AsyncGenericService>();
    server_builder_.reset(new ServerBuilder);
    ResourceQuota quota;
    quota.SetMaxThreads(concurrency_);
    server_builder_->SetResourceQuota(quota);
    server_builder_->RegisterAsyncGenericService(service_.get());
    // TODO: differentiate different address types.
    // TODO: add support for TLS.
    server_builder_->AddListeningPort(addr_, InsecureServerCredentials());
    // Each completion queue corresponds to +1 concurrency of the server.
    for (size_t i = 0; i < concurrency_; ++i) {
      completion_queues_.emplace_back(server_builder_->AddCompletionQueue());
    }
  } catch (bad_alloc& e) {
    auto execution_result =
        FailureExecutionResult(errors::SC_NETWORK_SERVICE_OOM);
    SCP_ERROR(kGrpcNetworkService, kZeroUuid, execution_result,
              "Network out of memory");
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult GrpcNetworkService::RegisterHandler(
    const string& uri, const RPCServiceContextInterface::RpcHandler& handler) {
  handlers_[uri] = handler;
  return SuccessExecutionResult();
}

ExecutionResult GrpcNetworkService::Run() noexcept {
  server_ = server_builder_->BuildAndStart();
  // TODO: add logging
  if (!server_) {
    auto execution_result =
        FailureExecutionResult(errors::SC_NETWORK_SERVICE_START_ERROR);
    SCP_ERROR(kGrpcNetworkService, kZeroUuid, execution_result,
              "Network service failed to start");
    return execution_result;
  }
  for (size_t i = 0; i < completion_queues_.size(); ++i) {
    mutex mtx;
    unique_lock<mutex> lock(mtx);
    condition_variable cv;
    pollers_.emplace_back(
        thread(&GrpcNetworkService::Worker, this, i, ref(mtx), ref(cv)));
    cv.wait(lock);
  }
  return SuccessExecutionResult();
}

ExecutionResult GrpcNetworkService::Stop() noexcept {
  server_->Shutdown();
  for (auto& queue : completion_queues_) {
    queue->Shutdown();
  }
  for (auto& t : pollers_) {
    if (t.joinable()) {
      t.join();
    }
  }
  return SuccessExecutionResult();
}

void GrpcNetworkService::Worker(size_t index, mutex& mtx,
                                condition_variable& cv) {
  auto queue = completion_queues_[index];
  GrpcTagManager<GrpcGenericContext> tag_manager;
  tag_manager.Allocate(queue, service_);
  // Unblock parent thread
  {
    scoped_lock<mutex> lock(mtx);
    cv.notify_one();
  }
  while (true) {
    bool ok = false;
    void* tag = nullptr;
    bool job_status = queue->Next(&tag, &ok);
    if (!job_status) {
      // The queue is shutdown. quit now
      SCP_INFO(kGrpcNetworkService, kZeroUuid, "The queue is shutdown.");
      return;
    }
    auto* ctx = static_cast<GrpcGenericContext*>(tag);
    if (!ok) {
      // The server is shutdown. If the queue returns any meaningful tag,
      // deallocate it.
      if (ctx) {
        tag_manager.Deallocate(ctx);
      }
      // Continue, do not return yet, as we may have more items in the queue
      // to drain.
      continue;
    }
    auto state = ctx->GetState();
    ctx->Process();
    if (state == GrpcGenericContext::State::kRead) {
      ctx->Read();
      // When a new request is ready, pre-allocate next one so that we keep
      // reading in the background.
      tag_manager.Allocate(queue, service_);
      ctx->SetState(GrpcGenericContext::State::kWrite);
      continue;
    }
    if (state == GrpcGenericContext::State::kWrite) {
      auto handler_iter = handlers_.find(ctx->Method());
      if (handler_iter == handlers_.end()) {
        SCP_INFO(kGrpcNetworkService, kZeroUuid, "Cannot find the handler.");
        ctx->HandleNotFound();
      } else {
        auto& handler = handler_iter->second;
        // Ideally, we may want to execute this in AsyncExecutor but it would
        // cause synchronization issues with the completion queue.
        // This at the moment calls GrpcHandler::Handle(), which will create
        // an AsyncContext and call the registered callback.
        handler(*ctx);
        ctx->SetState(GrpcGenericContext::State::kFinish);
      }
      continue;
    }
    if (state == GrpcGenericContext::State::kFinish) {
      ctx->Finish();
      ctx->SetState(GrpcGenericContext::State::kDestroy);
      continue;
    }
    // Here we must have state == kDestroy or unknown state.
    tag_manager.Deallocate(ctx);
  }  // while (true)
}

}  // namespace google::scp::core
