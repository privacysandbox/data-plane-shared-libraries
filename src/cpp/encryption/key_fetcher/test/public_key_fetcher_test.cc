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

#include "src/cpp/encryption/key_fetcher/src/public_key_fetcher.h"

#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "include/gtest/gtest.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/public_key_client/public_key_client_interface.h"

namespace privacy_sandbox::server_common {
namespace {

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;

class MockPublicKeyClient : public google::scp::cpio::PublicKeyClientInterface {
 public:
  ExecutionResult init_result_mock = SuccessExecutionResult();
  ExecutionResult Init() noexcept override { return init_result_mock; }

  ExecutionResult run_result_mock = SuccessExecutionResult();
  ExecutionResult Run() noexcept override { return run_result_mock; }

  ExecutionResult stop_result_mock = SuccessExecutionResult();
  ExecutionResult Stop() noexcept override { return stop_result_mock; }

  MOCK_METHOD(
      ExecutionResult, ListPublicKeys,
      (google::scp::cpio::ListPublicKeysRequest request,
       google::scp::cpio::Callback<google::scp::cpio::ListPublicKeysResponse>
           callback),
      (noexcept));
};

using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::ListPublicKeysRequest;
using google::scp::cpio::ListPublicKeysResponse;
using google::scp::cpio::PublicKey;
using google::scp::cpio::PublicKeyClientInterface;
using google::scp::cpio::PublicPrivateKeyPairId;
using testing::Return;

TEST(PublicKeyFetcherTest, SuccessfulRefresh) {
  std::unique_ptr<MockPublicKeyClient> mock_public_key_client =
      std::make_unique<MockPublicKeyClient>();

  ListPublicKeysResponse response;
  PublicKey key = {"key_id", "key_pubkey"};
  response.public_keys = {key};

  EXPECT_CALL(*mock_public_key_client, ListPublicKeys)
      .WillOnce(
          [&](ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback)
              -> ExecutionResult {
            callback(SuccessExecutionResult(), response);
            return SuccessExecutionResult();
          });

  PublicKeyFetcher fetcher(std::move(mock_public_key_client));
  absl::Status result_status = fetcher.Refresh([]() -> void {});
  EXPECT_TRUE(result_status.ok());

  std::vector<PublicPrivateKeyPairId> key_pair_ids;
  key_pair_ids.push_back(key.key_id);
  EXPECT_EQ(fetcher.GetKeyIds(), key_pair_ids);
}

TEST(PublicKeyFetcherTest, FailedSyncListPublicKeysCall) {
  std::unique_ptr<MockPublicKeyClient> mock_public_key_client =
      std::make_unique<MockPublicKeyClient>();

  EXPECT_CALL(*mock_public_key_client, ListPublicKeys)
      .WillOnce(
          [&](ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback)
              -> ExecutionResult { return FailureExecutionResult(0); });

  PublicKeyFetcher fetcher(std::move(mock_public_key_client));
  absl::Status result_status = fetcher.Refresh([]() -> void {});
  EXPECT_TRUE(IsUnavailable(result_status));
}

TEST(PublicKeyFetcherTest, FailedAsyncListPublicKeysCall) {
  std::unique_ptr<MockPublicKeyClient> mock_public_key_client =
      std::make_unique<MockPublicKeyClient>();

  ListPublicKeysResponse response;
  response.public_keys = {{"key_id", "key_pubkey"}};

  EXPECT_CALL(*mock_public_key_client, ListPublicKeys)
      .WillOnce(
          [&](ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback)
              -> ExecutionResult {
            callback(FailureExecutionResult(0), response);
            return SuccessExecutionResult();
          });

  PublicKeyFetcher fetcher(std::move(mock_public_key_client));
  absl::Status result_status = fetcher.Refresh([]() -> void {});
  // GetKeyIds() should return an empty list since the public_keys_ field
  // shouldn't be set in the callback in Refresh().
  EXPECT_EQ(fetcher.GetKeyIds(), std::vector<std::string>());
}

TEST(PublicKeyFetcherTest, VerifyGetKeyReturnsRandomKey) {
  std::unique_ptr<MockPublicKeyClient> mock_public_key_client =
      std::make_unique<MockPublicKeyClient>();

  ListPublicKeysResponse response;
  std::vector<PublicKey> keys;
  for (int i = 0; i < 100; i++) {
    std::string i_str = std::to_string(i);
    PublicKey key = {"key_id" + i_str, "key_pubkey"};
    keys.push_back(key);
  }
  response.public_keys = keys;

  EXPECT_CALL(*mock_public_key_client, ListPublicKeys)
      .WillOnce(
          [&](ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback)
              -> ExecutionResult {
            callback(SuccessExecutionResult(), response);
            return SuccessExecutionResult();
          });

  PublicKeyFetcher fetcher(std::move(mock_public_key_client));
  absl::Status result_status = fetcher.Refresh([]() -> void {});

  PublicKey key = fetcher.GetKey().value();
  for (int i = 0; i < 100; i++) {
    if (key.key_id != fetcher.GetKey()->key_id) {
      SUCCEED();
      return;
    }
  }

  FAIL();
}

}  // namespace
}  // namespace privacy_sandbox::server_common
