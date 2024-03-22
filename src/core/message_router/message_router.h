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

#ifndef CORE_MESSAGE_ROUTER_MESSAGE_ROUTER_H_
#define CORE_MESSAGE_ROUTER_MESSAGE_ROUTER_H_

#include <map>
#include <memory>
#include <string>

#include "google/protobuf/any.pb.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/message_router_interface.h"
#include "src/public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::core {
template <>
struct MessageTraits<google::protobuf::Any> {
  using TypeIdentifier = std::string;
};

/*! @copydoc MessageRouterInterface
 */
class MessageRouter : public MessageRouterInterface<google::protobuf::Any,
                                                    google::protobuf::Any> {
 public:
  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  void OnMessageReceived(
      const std::shared_ptr<
          AsyncContext<google::protobuf::Any, google::protobuf::Any>>&
          context) noexcept override ABSL_LOCKS_EXCLUDED(mu_);

  using AsyncAction = typename std::function<void(
      AsyncContext<google::protobuf::Any, google::protobuf::Any>&)>;
  ExecutionResult Subscribe(RequestTypeId request_type,
                            AsyncAction action) noexcept override
      ABSL_LOCKS_EXCLUDED(mu_);

 private:
  // TODO(b/229794047): Figures out a better way to store the request_type to
  // have a better performance.
  absl::flat_hash_map<std::string, AsyncAction> actions_ ABSL_GUARDED_BY(mu_);
  absl::Mutex mu_;
};
}  // namespace google::scp::core

#endif  // CORE_MESSAGE_ROUTER_MESSAGE_ROUTER_H_
