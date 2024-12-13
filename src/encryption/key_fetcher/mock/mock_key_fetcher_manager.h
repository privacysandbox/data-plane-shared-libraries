/*
 * Copyright 2023 Google LLC
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

#ifndef ENCRYPTION_KEY_FETCHER_MOCK_MOCK_KEY_FETCHER_MANAGER_H_
#define ENCRYPTION_KEY_FETCHER_MOCK_MOCK_KEY_FETCHER_MANAGER_H_

#include <gmock/gmock.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "include/gtest/gtest.h"
#include "src/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"
#include "src/public/cpio/interface/private_key_client/type_def.h"
#include "src/public/cpio/interface/public_key_client/public_key_client_interface.h"
#include "src/public/cpio/interface/type_def.h"

namespace privacy_sandbox::server_common {

// Implementation of KeyFetcherManagerInterface to be used for unit testing
// any classes that have an instance of KeyFetcherManager as a dependency.
class MockKeyFetcherManager : public KeyFetcherManagerInterface {
 public:
  virtual ~MockKeyFetcherManager() = default;

  MOCK_METHOD(
      absl::StatusOr<google::cmrt::sdk::public_key_service::v1::PublicKey>,
      GetPublicKey, (CloudPlatform cloud_platform), (noexcept));

  MOCK_METHOD(std::optional<PrivateKey>, GetPrivateKey,
              (const google::scp::cpio::PublicPrivateKeyPairId& key_id),
              (noexcept));

  MOCK_METHOD(absl::Status, Start, (), (noexcept));
};

}  // namespace privacy_sandbox::server_common

#endif  // ENCRYPTION_KEY_FETCHER_MOCK_MOCK_KEY_FETCHER_MANAGER_H_
