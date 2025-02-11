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

#include "src/encryption/key_fetcher/public_key_fetcher.h"

#include <gmock/gmock.h>

#include <string>
#include <utility>

#include "absl/status/status.h"
#include "include/gtest/gtest.h"
#include "src/encryption/key_fetcher/key_fetcher_utils.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/public_key_client/public_key_client_interface.h"

namespace privacy_sandbox::server_common {
namespace {

using ::google::scp::core::ExecutionResult;
using ::google::scp::core::FailureExecutionResult;
using ::google::scp::core::SuccessExecutionResult;
using ::testing::ContainsRegex;
using ::testing::IsEmpty;

class MockPublicKeyClient : public google::scp::cpio::PublicKeyClientInterface {
 public:
  absl::Status init_result_mock = absl::OkStatus();

  absl::Status Init() noexcept override { return init_result_mock; }

  absl::Status run_result_mock = absl::OkStatus();

  absl::Status Run() noexcept override { return run_result_mock; }

  absl::Status stop_result_mock = absl::OkStatus();

  absl::Status Stop() noexcept override { return stop_result_mock; }

  MOCK_METHOD(
      absl::Status, ListPublicKeys,
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
  std::unique_ptr<MockPublicKeyClient> mock_public_key_client_gcp =
      std::make_unique<MockPublicKeyClient>();
  std::unique_ptr<MockPublicKeyClient> mock_public_key_client_aws =
      std::make_unique<MockPublicKeyClient>();

  PublicKey key;
  key.set_key_id("0000000");
  key.set_public_key("key_pubkey");
  ListPublicKeysResponse response;
  response.mutable_public_keys()->Add(std::move(key));

  EXPECT_CALL(*mock_public_key_client_gcp, ListPublicKeys)
      .WillOnce(
          [&](ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback) {
            callback(SuccessExecutionResult(), response);
            return absl::OkStatus();
          });

  EXPECT_CALL(*mock_public_key_client_aws, ListPublicKeys)
      .WillOnce(
          [&](ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback) {
            ListPublicKeysResponse empty_response;
            callback(SuccessExecutionResult(), empty_response);
            return absl::OkStatus();
          });

  absl::flat_hash_map<
      CloudPlatform,
      std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>>
      public_key_clients;
  public_key_clients[CloudPlatform::kGcp] =
      std::move(mock_public_key_client_gcp);
  public_key_clients[CloudPlatform::kAws] =
      std::move(mock_public_key_client_aws);

  PublicKeyFetcher fetcher(std::move(public_key_clients));
  absl::Status result_status = fetcher.Refresh();
  EXPECT_TRUE(result_status.ok());

  std::vector<PublicPrivateKeyPairId> key_pair_ids;
  absl::StatusOr<std::string> ohttp_key_id =
      ToOhttpKeyId(response.public_keys().at(0).key_id());
  ASSERT_TRUE(ohttp_key_id.ok());
  key_pair_ids.push_back(*std::move(ohttp_key_id));
  EXPECT_EQ(fetcher.GetKeyIds(CloudPlatform::kGcp), key_pair_ids);
}

TEST(PublicKeyFetcherTest, SyncReturnsIfExecutionFailsSinglePlatform) {
  std::unique_ptr<MockPublicKeyClient> mock_public_key_client =
      std::make_unique<MockPublicKeyClient>();

  EXPECT_CALL(*mock_public_key_client, ListPublicKeys)
      .WillOnce(
          [&](ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback) {
            return absl::UnknownError("");
          });

  absl::flat_hash_map<
      CloudPlatform,
      std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>>
      public_key_clients;
  public_key_clients[CloudPlatform::kGcp] = std::move(mock_public_key_client);

  PublicKeyFetcher fetcher(std::move(public_key_clients));
  absl::Status result_status = fetcher.Refresh();
  EXPECT_TRUE(result_status.ok());
}

TEST(PublicKeyFetcherTest, SyncReturnsIfExecutionFailsMultiPlatform) {
  // This test has the GCP execution fail, but the AWS execution return
  // successfully and fetch a key successfully.

  std::unique_ptr<MockPublicKeyClient> mock_public_key_client_gcp =
      std::make_unique<MockPublicKeyClient>();
  std::unique_ptr<MockPublicKeyClient> mock_public_key_client_aws =
      std::make_unique<MockPublicKeyClient>();
  EXPECT_CALL(*mock_public_key_client_gcp, ListPublicKeys)
      .WillOnce(
          [&](ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback) {
            return absl::UnknownError("");
          });

  PublicKey key;
  key.set_key_id("key_id");
  key.set_public_key("key_pubkey");
  ListPublicKeysResponse response;
  response.mutable_public_keys()->Add(std::move(key));

  EXPECT_CALL(*mock_public_key_client_aws, ListPublicKeys)
      .WillOnce(
          [aws_response = response](
              ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback) {
            callback(SuccessExecutionResult(), aws_response);
            return absl::OkStatus();
          });

  absl::flat_hash_map<
      CloudPlatform,
      std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>>
      public_key_clients;
  public_key_clients[CloudPlatform::kGcp] =
      std::move(mock_public_key_client_gcp);
  public_key_clients[CloudPlatform::kAws] =
      std::move(mock_public_key_client_aws);

  PublicKeyFetcher fetcher(std::move(public_key_clients));
  absl::Status result_status = fetcher.Refresh();
  EXPECT_TRUE(result_status.ok());

  std::vector<PublicPrivateKeyPairId> key_pair_ids;
  absl::StatusOr<std::string> ohttp_key_id =
      ToOhttpKeyId(response.public_keys().at(0).key_id());
  ASSERT_TRUE(ohttp_key_id.ok());
  key_pair_ids.push_back(*std::move(ohttp_key_id));
  EXPECT_EQ(fetcher.GetKeyIds(CloudPlatform::kAws), key_pair_ids);
}

TEST(PublicKeyFetcherTest, FailedAsyncListPublicKeysCall) {
  std::unique_ptr<MockPublicKeyClient> mock_public_key_client_gcp =
      std::make_unique<MockPublicKeyClient>();
  std::unique_ptr<MockPublicKeyClient> mock_public_key_client_aws =
      std::make_unique<MockPublicKeyClient>();

  PublicKey key;
  key.set_key_id("key_id");
  key.set_public_key("key_pubkey");
  ListPublicKeysResponse response;
  response.mutable_public_keys()->Add(std::move(key));

  EXPECT_CALL(*mock_public_key_client_gcp, ListPublicKeys)
      .WillOnce(
          [gcp_response = response](
              ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback) {
            callback(FailureExecutionResult(0), gcp_response);
            return absl::OkStatus();
          });
  EXPECT_CALL(*mock_public_key_client_aws, ListPublicKeys)
      .WillOnce(
          [aws_response = response](
              ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback) {
            callback(FailureExecutionResult(0), aws_response);
            return absl::OkStatus();
          });
  absl::flat_hash_map<
      CloudPlatform,
      std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>>
      public_key_clients;
  public_key_clients[CloudPlatform::kGcp] =
      std::move(mock_public_key_client_gcp);
  public_key_clients[CloudPlatform::kAws] =
      std::move(mock_public_key_client_aws);

  PublicKeyFetcher fetcher(std::move(public_key_clients));
  absl::Status result_status = fetcher.Refresh();
  // GetKeyIds() should return an empty list since the public_keys_ field
  // shouldn't be set in the callback in Refresh().
  EXPECT_THAT(fetcher.GetKeyIds(CloudPlatform::kGcp), IsEmpty());
  EXPECT_THAT(fetcher.GetKeyIds(CloudPlatform::kAws), IsEmpty());
}

TEST(PublicKeyFetcherTest, GetKey_ReturnsRandomKey) {
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
              google::scp::cpio::Callback<ListPublicKeysResponse> callback) {
            callback(SuccessExecutionResult(), response);
            return absl::OkStatus();
          });

  absl::flat_hash_map<
      CloudPlatform,
      std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>>
      public_key_clients;
  public_key_clients[CloudPlatform::kGcp] = std::move(mock_public_key_client);

  PublicKeyFetcher fetcher(std::move(public_key_clients));
  absl::Status result_status = fetcher.Refresh();

  // Call GetKey() and validate the result isn't equal to another GetKey() call.
  PublicKey key = *fetcher.GetKey(CloudPlatform::kGcp);
  for (int i = 0; i < 100; i++) {
    if (key.key_id() != fetcher.GetKey(CloudPlatform::kGcp)->key_id()) {
      SUCCEED();
      return;
    }
  }

  FAIL();
}

TEST(PublicKeyFetcherTest, GetKeys_ErrorOnKeysNotFetchedForCloudPlatform) {
  std::unique_ptr<MockPublicKeyClient> mock_public_key_client =
      std::make_unique<MockPublicKeyClient>();

  ListPublicKeysResponse response;
  PublicKey key;
  key.set_key_id("00000000");
  key.set_public_key("key_pubkey");
  response.mutable_public_keys()->Add({std::move(key)});

  EXPECT_CALL(*mock_public_key_client, ListPublicKeys)
      .WillOnce(
          [&](ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback) {
            callback(SuccessExecutionResult(), response);
            return absl::OkStatus();
          });

  absl::flat_hash_map<
      CloudPlatform,
      std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>>
      public_key_clients;
  public_key_clients[CloudPlatform::kGcp] = std::move(mock_public_key_client);

  PublicKeyFetcher fetcher(std::move(public_key_clients));
  absl::Status result_status = fetcher.Refresh();

  // Expect an error when fetching an AWS key.
  absl::StatusOr<PublicKey> error = fetcher.GetKey(CloudPlatform::kAws);
  ASSERT_TRUE(!error.ok());
  EXPECT_THAT(error.status().message(),
              ContainsRegex("No public keys to return for cloud platform AWS"));
}

TEST(PublicKeyFetcherTest, GetKeys_ErrorOnNoKeysForAnyCloudPlatforms) {
  std::unique_ptr<MockPublicKeyClient> mock_public_key_client =
      std::make_unique<MockPublicKeyClient>();

  ListPublicKeysResponse response;
  EXPECT_CALL(*mock_public_key_client, ListPublicKeys)
      .WillOnce(
          [&](ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback) {
            callback(SuccessExecutionResult(), response);
            return absl::OkStatus();
          });

  absl::flat_hash_map<
      CloudPlatform,
      std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>>
      public_key_clients;
  public_key_clients[CloudPlatform::kGcp] = std::move(mock_public_key_client);

  PublicKeyFetcher fetcher(std::move(public_key_clients));
  absl::Status result_status = fetcher.Refresh();

  absl::StatusOr<PublicKey> error = fetcher.GetKey(CloudPlatform::kGcp);
  ASSERT_TRUE(!error.ok());
  EXPECT_THAT(error.status().message(),
              ContainsRegex("No public keys cached for any cloud platforms"));
}

TEST(PublicKeyFetcherTest, Refresh_KeepsCachedKeysOnEmptyKeyFetchResponse) {
  std::unique_ptr<MockPublicKeyClient> mock_public_key_client =
      std::make_unique<MockPublicKeyClient>();

  ListPublicKeysResponse first_response_with_keys, empty_response;
  std::vector<PublicKey> keys;
  PublicKey key;
  key.set_key_id("00000000");
  key.set_public_key("key_pubkey");
  keys.push_back(key);

  first_response_with_keys.mutable_public_keys()->Add(keys.begin(), keys.end());

  EXPECT_CALL(*mock_public_key_client, ListPublicKeys)
      .WillOnce(
          [&](ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback) {
            callback(SuccessExecutionResult(), first_response_with_keys);
            return absl::OkStatus();
          })
      .WillOnce(
          [&](ListPublicKeysRequest request,
              google::scp::cpio::Callback<ListPublicKeysResponse> callback) {
            callback(SuccessExecutionResult(), empty_response);
            return absl::OkStatus();
          });

  absl::flat_hash_map<
      CloudPlatform,
      std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>>
      public_key_clients;
  public_key_clients[CloudPlatform::kGcp] = std::move(mock_public_key_client);

  PublicKeyFetcher fetcher(std::move(public_key_clients));
  absl::Status first_refresh = fetcher.Refresh();
  ASSERT_TRUE(first_refresh.ok());
  absl::StatusOr<PublicKey> first_key = fetcher.GetKey(CloudPlatform::kGcp);
  ASSERT_TRUE(first_key.ok());

  absl::Status second_refresh = fetcher.Refresh();
  ASSERT_TRUE(second_refresh.ok());
  absl::StatusOr<PublicKey> second_key = fetcher.GetKey(CloudPlatform::kGcp);
  ASSERT_TRUE(second_key.ok());

  ASSERT_EQ(first_key->key_id(), second_key->key_id());
  ASSERT_EQ(first_key->public_key(), second_key->public_key());
}

}  // namespace
}  // namespace privacy_sandbox::server_common
