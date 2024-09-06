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

#include "public_key_client_provider.h"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <google/protobuf/util/time_util.h>

#include "absl/functional/bind_front.h"
#include "google/protobuf/any.pb.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/http_client_interface.h"
#include "src/core/interface/http_types.h"
#include "src/cpio/client_providers/interface/public_key_client_provider_interface.h"
#include "src/cpio/client_providers/interface/type_def.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/public_key_client/type_def.h"
#include "src/public/cpio/proto/public_key_service/v1/public_key_service.pb.h"
#include "src/util/status_macro/status_macros.h"

#include "error_codes.h"
#include "public_key_client_utils.h"

using google::cmrt::sdk::public_key_service::v1::ListPublicKeysRequest;
using google::cmrt::sdk::public_key_service::v1::ListPublicKeysResponse;
using google::cmrt::sdk::public_key_service::v1::PublicKey;
using google::protobuf::Any;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_PUBLIC_KEY_CLIENT_PROVIDER_ALL_URIS_REQUEST_PERFORM_FAILED;

namespace {
constexpr std::string_view kPublicKeyClientProvider = "PublicKeyClientProvider";
}

namespace google::scp::cpio::client_providers {

void PublicKeyClientProvider::OnListPublicKeys(
    AsyncContext<Any, Any> any_context) noexcept {
  auto request = std::make_shared<ListPublicKeysRequest>();
  any_context.request->UnpackTo(request.get());
  AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse> context(
      std::move(request),
      absl::bind_front(CallbackToPackAnyResponse<ListPublicKeysRequest,
                                                 ListPublicKeysResponse>,
                       any_context),
      any_context);
  context.result = ListPublicKeys(context).ok()
                       ? SuccessExecutionResult()
                       : FailureExecutionResult(SC_UNKNOWN);
}

absl::Status PublicKeyClientProvider::ListPublicKeys(
    AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse>&
        public_key_fetching_context) noexcept {
  // Use got_success_result and unfinished_counter to track whether get success
  // response and how many failed responses. Only return one response whether
  // success or failed.
  auto got_success_result = std::make_shared<std::atomic<bool>>(false);
  auto unfinished_counter = std::make_shared<std::atomic<size_t>>(
      public_key_client_options_.endpoints.size());

  absl::Status error = absl::InternalError(
      "Public key client failed to perform request for config endpoints.");
  for (auto uri : public_key_client_options_.endpoints) {
    auto shared_uri = std::make_shared<Uri>(uri);

    auto http_request = std::make_shared<HttpRequest>();
    http_request->method = HttpMethod::GET;
    http_request->path = std::move(shared_uri);

    AsyncContext<HttpRequest, HttpResponse> http_client_context(
        std::move(http_request),
        std::bind(&PublicKeyClientProvider::OnPerformRequestCallback, this,
                  public_key_fetching_context, std::placeholders::_1,
                  got_success_result, unfinished_counter),
        public_key_fetching_context);

    auto execution_result = http_client_->PerformRequest(http_client_context);
    if (execution_result.Successful()) {
      // If there is one URI PerformRequest() success, ListPublicKeys will
      // return success.
      error = absl::OkStatus();
    } else {
      SCP_ERROR_CONTEXT(kPublicKeyClientProvider, public_key_fetching_context,
                        execution_result,
                        "Failed to perform request with endpoint %s.",
                        uri.c_str());
    }
  }

  if (!error.ok()) {
    SCP_ERROR_CONTEXT(kPublicKeyClientProvider, public_key_fetching_context,
                      public_key_fetching_context.result,
                      "Failed to perform request with config endpoints.");
    public_key_fetching_context.Finish();
  }

  return error;
}

void ExecutionResultCheckingHelper(
    AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse>& context,
    const ExecutionResult& result,
    std::shared_ptr<std::atomic<size_t>> unfinished_counter) noexcept {
  auto pervious_unfinished = unfinished_counter->fetch_sub(1);
  if (pervious_unfinished == 1) {
    SCP_ERROR_CONTEXT(kPublicKeyClientProvider, context, context.result,
                      "Failed to fetch public keys.");
    context.Finish(result);
  }
}

void PublicKeyClientProvider::OnPerformRequestCallback(
    AsyncContext<ListPublicKeysRequest, ListPublicKeysResponse>&
        public_key_fetching_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context,
    std::shared_ptr<std::atomic<bool>> got_success_result,
    std::shared_ptr<std::atomic<size_t>> unfinished_counter) noexcept {
  if (got_success_result->load()) {
    return;
  }

  auto result = http_client_context.result;
  if (!result.Successful()) {
    return ExecutionResultCheckingHelper(public_key_fetching_context, result,
                                         unfinished_counter);
  }

  uint64_t expired_time_in_s;
  auto headers = *http_client_context.response->headers;
  result = PublicKeyClientUtils::ParseExpiredTimeFromHeaders(headers,
                                                             expired_time_in_s);
  if (!result.Successful()) {
    return ExecutionResultCheckingHelper(public_key_fetching_context, result,
                                         unfinished_counter);
  }

  std::vector<PublicKey> public_keys;
  auto body = http_client_context.response->body;
  result = PublicKeyClientUtils::ParsePublicKeysFromBody(body, public_keys);
  if (!result.Successful()) {
    return ExecutionResultCheckingHelper(public_key_fetching_context, result,
                                         unfinished_counter);
  }

  auto got_result = false;
  if (got_success_result->compare_exchange_strong(got_result, true)) {
    public_key_fetching_context.response =
        std::make_shared<ListPublicKeysResponse>();
    *public_key_fetching_context.response->mutable_expiration_time() =
        TimeUtil::SecondsToTimestamp(expired_time_in_s);
    public_key_fetching_context.response->mutable_public_keys()->Add(
        public_keys.begin(), public_keys.end());
    public_key_fetching_context.Finish(SuccessExecutionResult());
  }
}

absl::Nonnull<std::unique_ptr<PublicKeyClientProviderInterface>>
PublicKeyClientProviderFactory::Create(
    PublicKeyClientOptions options,
    absl::Nonnull<HttpClientInterface*> http_client) {
  return std::make_unique<PublicKeyClientProvider>(std::move(options),
                                                   http_client);
}

}  // namespace google::scp::cpio::client_providers
