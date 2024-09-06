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

#ifndef CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_CLIENT_PROVIDER_MOCK_MOCK_PRIVATE_KEY_CLIENT_PROVIDER_WITH_OVERRIDES_H_
#define CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_CLIENT_PROVIDER_MOCK_MOCK_PRIVATE_KEY_CLIENT_PROVIDER_WITH_OVERRIDES_H_

#include <memory>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "src/core/http2_client/mock/mock_http_client.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/kms_client_provider/mock/mock_kms_client_provider.h"
#include "src/cpio/client_providers/private_key_client_provider/private_key_client_provider.h"
#include "src/cpio/client_providers/private_key_client_provider/private_key_client_utils.h"
#include "src/cpio/client_providers/private_key_fetcher_provider/mock/mock_private_key_fetcher_provider.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers::mock {
class MockPrivateKeyClientProviderWithOverrides
    : public PrivateKeyClientProvider {
 public:
  explicit MockPrivateKeyClientProviderWithOverrides(
      PrivateKeyClientOptions private_key_client_options)
      : PrivateKeyClientProvider(
            std::move(private_key_client_options),
            std::make_unique<MockPrivateKeyFetcherProvider>(),
            std::make_unique<MockKmsClientProvider>()) {}

  std::function<absl::Status(
      core::AsyncContext<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest,
          cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>&)>
      list_private_keys_by_ids_mock;

  absl::Status list_private_keys_by_ids_result_mock = absl::UnknownError("");

  absl::Status ListPrivateKeys(
      core::AsyncContext<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest,
          cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>&
          context) noexcept override {
    if (list_private_keys_by_ids_mock) {
      return list_private_keys_by_ids_mock(context);
    }
    if (list_private_keys_by_ids_result_mock.ok()) {
      context.response = std::make_shared<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>();
      context.Finish(core::SuccessExecutionResult());
      return list_private_keys_by_ids_result_mock;
    }

    return PrivateKeyClientProvider::ListPrivateKeys(context);
  }

  MockKmsClientProvider& GetKmsClientProvider() {
    return dynamic_cast<MockKmsClientProvider&>(*kms_client_provider_);
  }

  MockPrivateKeyFetcherProvider& GetPrivateKeyFetcherProvider() {
    return dynamic_cast<MockPrivateKeyFetcherProvider&>(*private_key_fetcher_);
  }
};

}  // namespace google::scp::cpio::client_providers::mock

#endif  // CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_CLIENT_PROVIDER_MOCK_MOCK_PRIVATE_KEY_CLIENT_PROVIDER_WITH_OVERRIDES_H_
