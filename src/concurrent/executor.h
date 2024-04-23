//  Copyright 2023 Google LLC
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

#include "absl/functional/any_invocable.h"
#include "absl/time/time.h"

#ifndef CONCURRENT_EXECUTOR_H
#define CONCURRENT_EXECUTOR_H

namespace privacy_sandbox::server_common {

// ID of a task queued on the executor.
struct TaskId {
  intptr_t keys[2];
};

// The class implementing this interface executes methods asynchronously.
// The implementations differ based on the source of threads used for execution.
class Executor {
 public:
  // Polymorphic class => virtual destructor
  virtual ~Executor() = default;

  // Queues the AnyInvocable to be executed immediately.
  virtual void Run(absl::AnyInvocable<void()> closure) = 0;

  // Queues the AnyInvocable to be executed after a fixed amount of time. The
  // returned TaskId can be passed to the Cancel() method below to stop a
  // scheduled task from running.
  virtual TaskId RunAfter(absl::Duration duration,
                          absl::AnyInvocable<void()> closure) = 0;

  // Requests the cancellation of a task.
  // Returns false if the closure has already been scheduled to run.
  // Returns true if the closure has not been scheduled to run and was canceled.
  virtual bool Cancel(TaskId task_id) = 0;
};

}  // namespace privacy_sandbox::server_common

#endif  // CONCURRENT_EXECUTOR_H
