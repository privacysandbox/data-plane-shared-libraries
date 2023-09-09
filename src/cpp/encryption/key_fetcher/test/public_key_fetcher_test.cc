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
#include "src/cpp/encryption/key_fetcher/src/key_fetcher_utils.h"

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
      (google::cmrt::sdk::public_key_service::v1::ListPublicKeysRequest request,
       google::scp::cpio::Callback<
           google::cmrt::sdk::public_key_service::v1::ListPublicKeysResponse>
           callback),
      (noexcept));
};

using ::google::cmrt::sdk::public_key_service::v1::ListPublicKeysRequest;
using ::google::cmrt::sdk::public_key_service::v1::ListPublicKeysResponse;
using ::google::cmrt::sdk::public_key_service::v1::PublicKey;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::cpio::PublicKeyClientInterface;
using ::google::scp::cpio::PublicPrivateKeyPairId;
using ::testing::Return;

TEST(PublicKeyFetcherTest, SuccessfulRefresh) {
  std::unique_ptr<MockPublicKeyClient> mock_public_key_client =
      std::make_unique<MockPublicKeyClient>();

  PublicKey key;
  key.set_key_id("0000000");
  key.set_public_key("key_pubkey");
  ListPublicKeysResponse response;
  response.mutable_public_keys()->Add(std::move(key));

  EXPECT_CALL(*mock_public_key_client, ListPublicKeys)
      .WillOnce(
          [&](ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback)
              -> ExecutionResult {
            callback(SuccessExecutionResult(), response);
            return SuccessExecutionResult();
          });

  absl::flat_hash_map<
      CloudPlatform,
      std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>>
      public_key_clients;
  public_key_clients[CloudPlatform::GCP] = std::move(mock_public_key_client);

  PublicKeyFetcher fetcher(std::move(public_key_clients));
  absl::Status result_status = fetcher.Refresh();
  EXPECT_TRUE(result_status.ok());

  std::vector<PublicPrivateKeyPairId> key_pair_ids;
  key_pair_ids.push_back(ToOhttpKeyId(response.public_keys().at(0).key_id()));
  EXPECT_EQ(fetcher.GetKeyIds(CloudPlatform::GCP), key_pair_ids);
}

TEST(PublicKeyFetcherTest, FailedSyncListPublicKeysCall) {
  std::unique_ptr<MockPublicKeyClient> mock_public_key_client =
      std::make_unique<MockPublicKeyClient>();

  EXPECT_CALL(*mock_public_key_client, ListPublicKeys)
      .WillOnce(
          [&](ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback)
              -> ExecutionResult { return FailureExecutionResult(0); });

  absl::flat_hash_map<
      CloudPlatform,
      std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>>
      public_key_clients;
  public_key_clients[CloudPlatform::GCP] = std::move(mock_public_key_client);

  PublicKeyFetcher fetcher(std::move(public_key_clients));
  absl::Status result_status = fetcher.Refresh();
  EXPECT_TRUE(IsUnavailable(result_status));
}

TEST(PublicKeyFetcherTest, FailedAsyncListPublicKeysCall) {
  std::unique_ptr<MockPublicKeyClient> mock_public_key_client =
      std::make_unique<MockPublicKeyClient>();

  PublicKey key;
  key.set_key_id("key_id");
  key.set_public_key("key_pubkey");
  ListPublicKeysResponse response;
  response.mutable_public_keys()->Add(std::move(key));

  EXPECT_CALL(*mock_public_key_client, ListPublicKeys)
      .WillOnce(
          [&](ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback)
              -> ExecutionResult {
            callback(FailureExecutionResult(0), response);
            return SuccessExecutionResult();
          });

  absl::flat_hash_map<
      CloudPlatform,
      std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>>
      public_key_clients;
  public_key_clients[CloudPlatform::GCP] = std::move(mock_public_key_client);

  PublicKeyFetcher fetcher(std::move(public_key_clients));
  absl::Status result_status = fetcher.Refresh();
  // GetKeyIds() should return an empty list since the public_keys_ field
  // shouldn't be set in the callback in Refresh().
  EXPECT_EQ(fetcher.GetKeyIds(CloudPlatform::GCP), std::vector<std::string>());
}

TEST(PublicKeyFetcherTest, VerifyGetKeyReturnsRandomKey) {
  std::unique_ptr<MockPublicKeyClient> mock_public_key_client =
      std::make_unique<MockPublicKeyClient>();

  ListPublicKeysResponse response;
  std::vector<PublicKey> keys;
  for (int i = 0; i < 100; i++) {
    std::string i_str = std::to_string(i);
    PublicKey key;
    key.set_key_id(i_str + "00000000");
    key.set_public_key("key_pubkey");
    keys.push_back(key);
  }

  response.mutable_public_keys()->Add(keys.begin(), keys.end());

  EXPECT_CALL(*mock_public_key_client, ListPublicKeys)
      .WillOnce(
          [&](ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback)
              -> ExecutionResult {
            callback(SuccessExecutionResult(), response);
            return SuccessExecutionResult();
          });

  absl::flat_hash_map<
      CloudPlatform,
      std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>>
      public_key_clients;
  public_key_clients[CloudPlatform::GCP] = std::move(mock_public_key_client);

  PublicKeyFetcher fetcher(std::move(public_key_clients));
  absl::Status result_status = fetcher.Refresh();

  // Call GetKey() and validate the result isn't equal to another GetKey() call.
  PublicKey key = *fetcher.GetKey(CloudPlatform::GCP);
  for (int i = 0; i < 100; i++) {
    if (key.key_id() != fetcher.GetKey(CloudPlatform::GCP)->key_id()) {
      SUCCEED();
      return;
    }
  }

  FAIL();
}

}  // namespace
}  // namespace privacy_sandbox::server_common
