/*
 * Copyright 2024 Google LLC
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

#ifndef PRIVACY_SANDBOX_SERVER_COMMON_BYOB_H_
#define PRIVACY_SANDBOX_SERVER_COMMON_BYOB_H_

#include <memory>
#include <string_view>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "src/roma/byob/example/example.pb.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/util/execution_token.h"

namespace privacy_sandbox::server_common::byob::example {

/*
 * service: privacy_sandbox.server_common.byob.example.EchoService
 */
template <typename TMetadata = google::scp::roma::DefaultMetadata>
class EchoService {
 public:
  /*
   * Echo
   *
   * request: ::privacy_sandbox::server_common::byob::example::EchoRequest
   * response: ::privacy_sandbox::server_common::byob::example::EchoResponse
   */
  virtual absl::StatusOr<google::scp::roma::ExecutionToken> Echo(
      absl::Notification& notification,
      const ::privacy_sandbox::server_common::byob::example::EchoRequest&
          request,
      absl::StatusOr<std::unique_ptr<
          ::privacy_sandbox::server_common::byob::example::EchoResponse>>&
          response,
      TMetadata metadata = TMetadata(), std::string_view code_token = "") = 0;

  virtual absl::StatusOr<google::scp::roma::ExecutionToken> Echo(
      absl::AnyInvocable<
          void(absl::StatusOr<
               ::privacy_sandbox::server_common::byob::example::EchoResponse>)>
          callback,
      const ::privacy_sandbox::server_common::byob::example::EchoRequest&
          request,
      TMetadata metadata = TMetadata(), std::string_view code_token = "") = 0;
};

}  // namespace privacy_sandbox::server_common::byob::example

#endif  // PRIVACY_SANDBOX_SERVER_COMMON_BYOB_H_
