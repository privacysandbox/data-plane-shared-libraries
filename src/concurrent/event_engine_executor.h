//  Copyright 2022 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#ifndef CONCURRENT_EVENT_ENGINE_EXECUTOR_H_
#define CONCURRENT_EVENT_ENGINE_EXECUTOR_H_

#include <memory>
#include <utility>

#include "absl/time/time.h"
#include "grpc/grpc.h"
#include "include/grpc/event_engine/event_engine.h"

#include "executor.h"

namespace privacy_sandbox::server_common {

// This class implements the Executor interface and executes methods
// asynchronously using a provided gRPC event engine task engine
// This class is not the owner of the event engine object and acts
// merely as a wrapper to prevent access to other parts of event engine.
class EventEngineExecutor : public Executor {
 public:
  explicit EventEngineExecutor(
      std::shared_ptr<grpc_event_engine::experimental::EventEngine>
          event_engine)
      : event_engine_(std::move(event_engine)) {}

  // EventEngineExecutor is neither copyable nor movable.
  EventEngineExecutor(const EventEngineExecutor&) = delete;
  EventEngineExecutor& operator=(const EventEngineExecutor&) = delete;

  // Executes synchronous task as soon as possible.
  void Run(absl::AnyInvocable<void()> closure) override;

  // Schedules a task to run after the specified duration. The returned TaskId
  // can be passed to the Cancel() method below to stop a scheduled task from
  // running.
  // Note: The closure passed to the method is deleted after it has been run.
  TaskId RunAfter(absl::Duration duration,
                  absl::AnyInvocable<void()> closure) override;

  // Requests the cancellation of a task.
  // Returns false if the closure has already been scheduled to run.
  // Returns true if the closure has not been scheduled to run and was canceled.
  bool Cancel(TaskId task_id) override;

 private:
  // own EventEngine for the lifetime of EventEngineExecutor
  std::shared_ptr<grpc_event_engine::experimental::EventEngine> event_engine_;
};

// Keep an GrpcInit object alive for the duration of grpc usage.
struct GrpcInit {
  GrpcInit() { grpc_init(); }

  ~GrpcInit() { grpc_shutdown(); }
};

}  // namespace privacy_sandbox::server_common

#endif  // CONCURRENT_EVENT_ENGINE_EXECUTOR_H_
