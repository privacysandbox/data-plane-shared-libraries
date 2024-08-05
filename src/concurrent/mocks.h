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

#ifndef CONCURRENT_MOCKS_H_
#define CONCURRENT_MOCKS_H_

#include <gmock/gmock.h>

#include <memory>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "include/grpc/event_engine/event_engine.h"

#include "event_engine_executor.h"

namespace privacy_sandbox::server_common {

class MockEventEngine : public grpc_event_engine::experimental::EventEngine {
 public:
  MOCK_METHOD(void, Run, (absl::AnyInvocable<void()> closure), (override));
  MOCK_METHOD(void, Run,
              (grpc_event_engine::experimental::EventEngine::Closure * closure),
              (override));
  MOCK_METHOD(grpc_event_engine::experimental::EventEngine::TaskHandle,
              RunAfter,
              (grpc_event_engine::experimental::EventEngine::Duration when,
               absl::AnyInvocable<void()> closure),
              (override));
  MOCK_METHOD(grpc_event_engine::experimental::EventEngine::TaskHandle,
              RunAfter,
              (grpc_event_engine::experimental::EventEngine::Duration when,
               grpc_event_engine::experimental::EventEngine::Closure* closure),
              (override));
  MOCK_METHOD(
      absl::StatusOr<std::unique_ptr<Listener>>, CreateListener,
      (Listener::AcceptCallback on_accept,
       absl::AnyInvocable<void(absl::Status)> on_shutdown,
       const grpc_event_engine::experimental::EndpointConfig& config,
       std::unique_ptr<grpc_event_engine::experimental::MemoryAllocatorFactory>
           memory_allocator_factory),
      (override));
  MOCK_METHOD(
      grpc_event_engine::experimental::EventEngine::ConnectionHandle, Connect,
      (grpc_event_engine::experimental::EventEngine::OnConnectCallback
           on_connect,
       const grpc_event_engine::experimental::EventEngine::ResolvedAddress&
           addr,
       const grpc_event_engine::experimental::EndpointConfig& args,
       grpc_event_engine::experimental::MemoryAllocator memory_allocator,
       grpc_event_engine::experimental::EventEngine::Duration timeout),
      (override));
  MOCK_METHOD(bool, Cancel,
              (grpc_event_engine::experimental::EventEngine::TaskHandle handle),
              (override));
  MOCK_METHOD(
      bool, CancelConnect,
      (grpc_event_engine::experimental::EventEngine::ConnectionHandle handle),
      (override));
  MOCK_METHOD(bool, IsWorkerThread, (), (override));
  MOCK_METHOD(absl::StatusOr<std::unique_ptr<
                  grpc_event_engine::experimental::EventEngine::DNSResolver>>,
              GetDNSResolver,
              (const grpc_event_engine::experimental::EventEngine::DNSResolver::
                   ResolverOptions& options),
              (override));
};

}  // namespace privacy_sandbox::server_common

#endif  // CONCURRENT_MOCKS_H_
