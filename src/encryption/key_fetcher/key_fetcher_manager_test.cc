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

#include "absl/synchronization/blocking_counter.h"
#include "absl/time/clock.h"
#include "googletest/include/gtest/gtest.h"
#include "include/grpc/event_engine/event_engine.h"
#include "include/gtest/gtest.h"
#include "src/concurrent/event_engine_executor.h"
#include "src/concurrent/executor.h"
#include "src/encryption/key_fetcher/interface/private_key_fetcher_interface.h"
#include "src/encryption/key_fetcher/mock/mock_private_key_fetcher.h"
#include "src/encryption/key_fetcher/mock/mock_public_key_fetcher.h"

namespace privacy_sandbox::server_common {
namespace {

class KeyFetcherManagerTest : public ::testing::Test {
 protected:
  KeyFetcherManagerTest() {
    grpc_init();
    event_engine_ = grpc_event_engine::experimental::CreateEventEngine();
    grpc_shutdown();
    executor_ = std::make_shared<EventEngineExecutor>(std::move(event_engine_));
  }

  std::unique_ptr<grpc_event_engine::experimental::EventEngine> event_engine_;
  std::shared_ptr<Executor> executor_;
};

TEST_F(KeyFetcherManagerTest, SuccessfulRefresh) {
  std::unique_ptr<MockPublicKeyFetcher> public_key_fetcher =
      std::make_unique<MockPublicKeyFetcher>();
  std::unique_ptr<MockPrivateKeyFetcher> private_key_fetcher =
      std::make_unique<MockPrivateKeyFetcher>();

  std::vector<std::string> key_ids = {"key_id"};
  EXPECT_CALL(*public_key_fetcher, Refresh)
      .WillRepeatedly([&]() -> absl::Status { return absl::OkStatus(); });
  EXPECT_CALL(*public_key_fetcher, GetKeyIds)
      .WillRepeatedly(
          [&]() -> std::vector<google::scp::cpio::PublicPrivateKeyPairId> {
            return key_ids;
          });
  EXPECT_CALL(*private_key_fetcher, Refresh)
      .WillRepeatedly([&]() -> absl::Status { return absl::OkStatus(); });

  KeyFetcherManager manager(absl::Minutes(1), std::move(public_key_fetcher),
                            std::move(private_key_fetcher),
                            std::move(executor_));
  manager.Start();
  // Sleep so the code can finish executing on the background thread.
  absl::SleepFor(absl::Milliseconds(5));
}

TEST_F(KeyFetcherManagerTest, SuccessfulPublicKeyRefreshNoPrivateKeysToFetch) {
  std::unique_ptr<MockPublicKeyFetcher> public_key_fetcher =
      std::make_unique<MockPublicKeyFetcher>();
  std::unique_ptr<MockPrivateKeyFetcher> private_key_fetcher =
      std::make_unique<MockPrivateKeyFetcher>();

  std::vector<std::string> key_ids = {"key_id"};
  EXPECT_CALL(*public_key_fetcher, Refresh)
      .WillRepeatedly([&]() -> absl::Status { return absl::OkStatus(); });
  EXPECT_CALL(*public_key_fetcher, GetKeyIds)
      .WillRepeatedly(
          [&]() -> std::vector<google::scp::cpio::PublicPrivateKeyPairId> {
            return key_ids;
          });
  EXPECT_CALL(*private_key_fetcher, GetKey)
      .WillRepeatedly(
          [&](const google::scp::cpio::PublicPrivateKeyPairId key_id)
              -> std::optional<PrivateKey> {
            return std::optional<PrivateKey>();
          });
  EXPECT_CALL(*private_key_fetcher, Refresh)
      .WillRepeatedly([&]() -> absl::Status { return absl::OkStatus(); });

  KeyFetcherManager manager(absl::Minutes(1), std::move(public_key_fetcher),
                            std::move(private_key_fetcher),
                            std::move(executor_));
  manager.Start();
  // Sleep so the code can finish executing on the background thread.
  absl::SleepFor(absl::Milliseconds(5));
}

TEST_F(KeyFetcherManagerTest, NullPointerForPublicKeyFetcher) {
  std::unique_ptr<MockPrivateKeyFetcher> private_key_fetcher =
      std::make_unique<MockPrivateKeyFetcher>();
  EXPECT_CALL(*private_key_fetcher, Refresh).WillRepeatedly([]() {
    return absl::OkStatus();
  });

  KeyFetcherManager manager(absl::Minutes(1), /* public_key_fetcher= */ nullptr,
                            std::move(private_key_fetcher),
                            std::move(executor_));
  manager.Start();
}

}  // namespace
}  // namespace privacy_sandbox::server_common
