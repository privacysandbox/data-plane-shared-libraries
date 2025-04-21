/*
 * Copyright 2025 Google LLC
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

#ifndef SRC_CLIENTS_ASYNC_CLIENT_MOCKS_H_
#define SRC_CLIENTS_ASYNC_CLIENT_MOCKS_H_

#include <gmock/gmock.h>

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "src/clients/async_client/async_http_client.h"

namespace privacy_sandbox::server_common::clients {

template <typename Request, typename Response, typename RawRequest = Request,
          typename RawResponse = Response>
class AsyncClientMock
    : public AsyncClient<Request, Response, RawRequest, RawResponse> {
 public:
  MOCK_METHOD(
      absl::Status, Execute,
      (std::unique_ptr<Request> request, const RequestMetadata& metadata,
       absl::AnyInvocable<void(absl::StatusOr<std::unique_ptr<Response>>) &&>
           on_done,
       absl::Duration timeout, RequestContext context),
      (const, override));

  using OnDoneCallbackType =
      absl::AnyInvocable<void(absl::StatusOr<std::unique_ptr<RawResponse>>,
                              ResponseMetadata) &&>;
  MOCK_METHOD(absl::Status, ExecuteInternal,
              (std::unique_ptr<RawRequest> raw_request,
               grpc::ClientContext* context, OnDoneCallbackType on_done,
               absl::Duration timeout, RequestConfig request_config),
              (override));
};

}  // namespace privacy_sandbox::server_common::clients

#endif  // SRC_CLIENTS_ASYNC_CLIENT_MOCKS_H_
