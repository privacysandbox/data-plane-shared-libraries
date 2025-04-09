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

#ifndef SRC_CORE_KV_CLIENT_BYOS_CLIENT_H_
#define SRC_CORE_KV_CLIENT_BYOS_CLIENT_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"

namespace privacy_sandbox::server_common::kv_client {

// This provides access to the Metadata Object type
using RequestMetadata = absl::flat_hash_map<std::string, std::string>;

// Interface for client that fetches Key/Value pairs from
// a BYOS KV server
// TODO(b/411430242): This could be AsyncClient from B&A repo
template <typename ByosInput, typename ByosOutput>
class ByosClient {
 public:
  virtual ~ByosClient() = default;
  // Executes the request to a Key-Value Server asynchronously.
  //
  // keys: the request object to execute (the keys to query).
  // on_done: callback called when the request is finished executing.
  // timeout: a timeout value for the request.
  virtual absl::Status Execute(
      std::unique_ptr<ByosInput> keys, const RequestMetadata& metadata,
      absl::AnyInvocable<void(absl::StatusOr<std::unique_ptr<ByosOutput>>) &&>
          on_done,
      absl::Duration timeout,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext)) const = 0;
};

}  // namespace privacy_sandbox::server_common::kv_client

#endif  // SRC_CORE_KV_CLIENT_BYOS_CLIENT_H_
