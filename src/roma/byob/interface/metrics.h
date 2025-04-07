/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_ROMA_BYOB_INTERFACE_METRICS_H_
#define SRC_ROMA_BYOB_INTERFACE_METRICS_H_

#include "absl/time/time.h"

namespace privacy_sandbox::server_common::byob {
struct ProcessRequestMetrics {
  // Time spent waiting for a worker.
  absl::Duration wait_time;
  // Latency of `SerializeDelimitedToFileDescriptor` call that sends request.
  absl::Duration send_time;
  // Time from after `SerializeDelimitedToFileDescriptor` to
  // `ParseDelimitedFromZeroCopyStream` call completion.
  absl::Duration response_time;
  // The number of unused workers for a given code token.
  int unused_workers;
};
}  // namespace privacy_sandbox::server_common::byob

#endif  // SRC_ROMA_BYOB_INTERFACE_METRICS_H_
