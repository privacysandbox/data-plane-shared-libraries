// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/encryption/key_fetcher/key_fetcher_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <grpc/event_engine/event_engine.h>
#include <grpc/grpc.h>

#include "absl/strings/match.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/time/clock.h"
#include "googletest/include/gtest/gtest.h"
#include "include/gtest/gtest.h"
#include "src/encryption/key_fetcher/interface/private_key_fetcher_interface.h"
#include "src/encryption/key_fetcher/mock/mock_private_key_fetcher.h"
#include "src/encryption/key_fetcher/mock/mock_public_key_fetcher.h"

namespace privacy_sandbox::server_common {
namespace {

class KeyFetcherManagerTest : public ::testing::Test {
 protected:
  KeyFetcherManagerTest() {
    public_key_fetcher_ = std::make_unique<MockPublicKeyFetcher>();
    private_key_fetcher_ = std::make_unique<MockPrivateKeyFetcher>();
  }

  std::unique_ptr<MockPublicKeyFetcher> public_key_fetcher_;
  std::unique_ptr<MockPrivateKeyFetcher> private_key_fetcher_;
};

TEST_F(KeyFetcherManagerTest, SuccessfulRefresh) {
  EXPECT_CALL(*public_key_fetcher_, Refresh).WillOnce([&]() -> absl::Status {
    return absl::OkStatus();
  });
  EXPECT_CALL(*private_key_fetcher_, Refresh).WillOnce([&]() -> absl::Status {
    return absl::OkStatus();
  });

  KeyFetcherManager manager(absl::Minutes(1), std::move(public_key_fetcher_),
                            std::move(private_key_fetcher_));

  auto start_result = manager.Start();
  ASSERT_TRUE(start_result.ok());
}

TEST_F(KeyFetcherManagerTest, NullPointerForPublicKeyFetcher) {
  EXPECT_CALL(*private_key_fetcher_, Refresh).WillOnce([]() {
    return absl::OkStatus();
  });

  KeyFetcherManager manager(absl::Minutes(1), /* public_key_fetcher= */ nullptr,
                            std::move(private_key_fetcher_));

  auto start_result = manager.Start();
  ASSERT_TRUE(start_result.ok());
}

TEST_F(KeyFetcherManagerTest, ValidateErrorMessageOnPublicKeyFetchFailure) {
  EXPECT_CALL(*public_key_fetcher_, Refresh).WillOnce([&]() -> absl::Status {
    return absl::InternalError("public key fetch failed");
  });
  EXPECT_CALL(*private_key_fetcher_, Refresh).WillOnce([&]() -> absl::Status {
    return absl::OkStatus();
  });

  KeyFetcherManager manager(absl::Minutes(1), std::move(public_key_fetcher_),
                            std::move(private_key_fetcher_));

  auto start_result = manager.Start();
  ASSERT_FALSE(start_result.ok());
  ASSERT_TRUE(
      absl::StrContains(start_result.message(), "public key fetch failed"));
}

TEST_F(KeyFetcherManagerTest, ValidateErrorMessageOnPrivateKeyFetchFailure) {
  EXPECT_CALL(*public_key_fetcher_, Refresh).WillOnce([&]() -> absl::Status {
    return absl::OkStatus();
  });
  EXPECT_CALL(*private_key_fetcher_, Refresh).WillOnce([&]() -> absl::Status {
    return absl::InternalError("private key fetch failed");
  });

  KeyFetcherManager manager(absl::Minutes(1), std::move(public_key_fetcher_),
                            std::move(private_key_fetcher_));

  auto start_result = manager.Start();
  ASSERT_FALSE(start_result.ok());
  ASSERT_TRUE(
      absl::StrContains(start_result.message(), "private key fetch failed"));
}

TEST_F(KeyFetcherManagerTest,
       ValidateErrorMessageOnPublicAndPrivateKeyFetchFailure) {
  EXPECT_CALL(*public_key_fetcher_, Refresh).WillOnce([&]() -> absl::Status {
    return absl::InternalError("public key fetch failed");
  });
  EXPECT_CALL(*private_key_fetcher_, Refresh).WillOnce([&]() -> absl::Status {
    return absl::InternalError("private key fetch failed");
  });

  KeyFetcherManager manager(absl::Minutes(1), std::move(public_key_fetcher_),
                            std::move(private_key_fetcher_));

  auto start_result = manager.Start();
  ASSERT_FALSE(start_result.ok());
  ASSERT_TRUE(
      absl::StrContains(start_result.message(), "public key fetch failed"));
  ASSERT_TRUE(
      absl::StrContains(start_result.message(), "private key fetch failed"));
}

}  // namespace
}  // namespace privacy_sandbox::server_common
