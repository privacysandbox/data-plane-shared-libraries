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

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <grpcpp/completion_queue.h>
#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include "core/async_executor/src/async_executor.h"
#include "core/interface/network_service_interface.h"
#include "public/core/interface/execution_result.h"

#include "grpc_generic_context.h"
#include "grpc_tag_manager.h"

namespace google::scp::core {
/**
 * @brief The network service based on gRPC. This class intends to provide
 * generic interfaces for URI handlers.
 *
 */
class GrpcNetworkService : public NetworkServiceInterface {
 public:
  GrpcNetworkService(AddressType addr_type, Address addr,
                     size_t concurrency = 0UL)
      : addr_type_(addr_type),
        addr_(addr),
        concurrency_(concurrency == 0UL ? std::thread::hardware_concurrency()
                                        : concurrency) {}

  ExecutionResult Init() noexcept override;
  ExecutionResult Run() noexcept override;
  ExecutionResult Stop() noexcept override;

  // TODO: to remove or to implement.
  ExecutionResult Listen(const Address&) override {
    return SuccessExecutionResult();
  }

  ExecutionResult RegisterHandler(
      const std::string& uri,
      const RPCServiceContextInterface::RpcHandler& handler) override;

  /// The worker of this service. They are spawned as threads in \a pollers_.
  /// @param index The index of the completion queue in \a completion_queues_ to
  /// use. Each worker will keep working exclusively on its own queue to avoid
  /// contention.
  /// @param mutex The mutex to lock/unlock when notifying \a cv
  /// @param cv The conditional variable to notify when it is safe for the
  /// parent thread to proceed.
  void Worker(size_t index, std::mutex& mutex, std::condition_variable& cv);

 private:
  /// The type of the address, i.e. host:port or socket_path or... See
  /// AddressType for details.
  const AddressType addr_type_;
  /// The address to listen on.
  const std::string addr_;
  /// The number of threads to run, for each GRPCServer object. This is not
  /// adjustable after construction at the moment, so we declare it as const.
  const size_t concurrency_;
  /// The completion queues that the requests and responses are going through.
  std::vector<std::shared_ptr<::grpc::ServerCompletionQueue>>
      completion_queues_;
  /// Only one generic service is allowed per server, and we only need one.
  std::shared_ptr<grpc::AsyncGenericService> service_;
  /// The map of uri to handlers. This map should be read only after run. Hence
  /// no locking is in place at the moment.
  std::unordered_map<std::string, RPCServiceContextInterface::RpcHandler>
      handlers_;
  std::unique_ptr<grpc::ServerBuilder> server_builder_;
  std::unique_ptr<grpc::Server> server_;
  /// The internal poller threads for reading requests.
  std::vector<std::thread> pollers_;
};
}  // namespace google::scp::core
