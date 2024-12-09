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

#include "event_engine_executor.h"

#include <utility>

#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace privacy_sandbox::server_common {

void EventEngineExecutor::Run(absl::AnyInvocable<void()> closure) {
  event_engine_->Run(std::move(closure));
}

TaskId EventEngineExecutor::RunAfter(const absl::Duration duration,
                                     absl::AnyInvocable<void()> closure) {
  grpc_event_engine::experimental::EventEngine::TaskHandle task_handle =
      event_engine_->RunAfter(ToChronoNanoseconds(duration),
                              std::move(closure));
  return {
      .keys = {task_handle.keys[0], task_handle.keys[1]},
  };
}

bool EventEngineExecutor::Cancel(TaskId task_id) {
  return event_engine_->Cancel(
      grpc_event_engine::experimental::EventEngine::TaskHandle{
          .keys = {task_id.keys[0], task_id.keys[1]},
      });
}

}  // namespace privacy_sandbox::server_common
