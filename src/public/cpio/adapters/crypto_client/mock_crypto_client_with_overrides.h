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

#ifndef PUBLIC_CPIO_ADAPTERS_CRYPTO_CLIENT_MOCK_CRYPTO_CLIENT_WITH_OVERRIDES_H_
#define PUBLIC_CPIO_ADAPTERS_CRYPTO_CLIENT_MOCK_CRYPTO_CLIENT_WITH_OVERRIDES_H_

#include <memory>

#include "src/cpio/client_providers/crypto_client_provider/mock/mock_crypto_client_provider.h"

namespace google::scp::cpio::mock {
class MockCryptoClientWithOverrides final : public CryptoClient {
 public:
  MockCryptoClientWithOverrides()
      : CryptoClient(std::make_unique<
                     client_providers::mock::MockCryptoClientProvider>()) {}

  client_providers::mock::MockCryptoClientProvider& GetCryptoClientProvider() {
    return dynamic_cast<client_providers::mock::MockCryptoClientProvider&>(
        *crypto_client_provider_);
  }
};
}  // namespace google::scp::cpio::mock

#endif  // PUBLIC_CPIO_ADAPTERS_CRYPTO_CLIENT_MOCK_CRYPTO_CLIENT_WITH_OVERRIDES_H_
