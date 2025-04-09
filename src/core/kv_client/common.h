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

#ifndef SRC_CORE_KV_CLIENT_COMMON_H_
#define SRC_CORE_KV_CLIENT_COMMON_H_

#include "absl/container/btree_set.h"

namespace privacy_sandbox::server_common::kv_client {

using UrlKeysSet = absl::btree_set<absl::string_view>;

enum class ClientType : int {
  CLIENT_TYPE_UNKNOWN = 0,
  CLIENT_TYPE_ANDROID = 1,
  CLIENT_TYPE_BROWSER = 2,
};

}  // namespace privacy_sandbox::server_common::kv_client

#endif  // SRC_CORE_KV_CLIENT_COMMON_H_
