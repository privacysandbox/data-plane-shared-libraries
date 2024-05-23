/*
 * Copyright 2023 Google LLC
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

#include "src/logger/request_context_impl.h"

namespace privacy_sandbox::server_common::log {

void AddEventMessage(const ::google::protobuf::Message& msg,
                     absl::AnyInvocable<DebugInfo*()>& debug_info) {
  // no-op for prod
}

bool IsProd() { return true; }

}  // namespace privacy_sandbox::server_common::log
