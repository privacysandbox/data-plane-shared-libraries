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

#include <gmock/gmock.h>

#include <vector>

#include "include/gtest/gtest.h"
#include "src/encryption/key_fetcher/interface/private_key_fetcher_interface.h"
#include "src/public/cpio/interface/private_key_client/type_def.h"
#include "src/public/cpio/interface/type_def.h"

namespace privacy_sandbox::server_common {

// Implementation of PrivateKeyFetcherInterface to be used for unit testing any
// classes that have an instance of PrivateKeyFetcher as a dependency.
class MockPrivateKeyFetcher : public PrivateKeyFetcherInterface {
 public:
  virtual ~MockPrivateKeyFetcher() = default;

  MOCK_METHOD(absl::Status, Refresh, (), (noexcept));

  MOCK_METHOD(std::optional<PrivateKey>, GetKey,
              (const google::scp::cpio::PublicPrivateKeyPairId&), (noexcept));
};

}  // namespace privacy_sandbox::server_common
