
// Copyright 2022 Google LLC
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

#include "src/cpio/client_providers/private_key_client_provider/private_key_client_provider.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/util/time_util.h>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/notification.h"
#include "src/core/interface/async_context.h"
#include "src/core/test/utils/proto_test_utils.h"
#include "src/core/test/utils/timestamp_test_utils.h"
#include "src/core/utils/base64.h"
#include "src/cpio/client_providers/kms_client_provider/mock/mock_kms_client_provider.h"
#include "src/cpio/client_providers/private_key_client_provider/mock/mock_private_key_client_provider_with_overrides.h"
#include "src/cpio/client_providers/private_key_client_provider/private_key_client_utils.h"
#include "src/cpio/client_providers/private_key_fetcher_provider/mock/mock_private_key_fetcher_provider.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"
#include "src/public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

namespace google::scp::cpio::client_providers::test {
namespace {

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse;
using google::cmrt::sdk::private_key_service::v1::PrivateKey;
using google::protobuf::Any;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::
    SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_KEY_DATA_COUNT;
using google::scp::core::errors::
    SC_PRIVATE_KEY_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_CLIENT_PROVIDER_UNMATCHED_ENDPOINTS_SPLITS;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::ExpectTimestampEquals;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::utils::Base64Encode;
using google::scp::cpio::client_providers::mock::MockKmsClientProvider;
using google::scp::cpio::client_providers::mock::
    MockPrivateKeyClientProviderWithOverrides;
using google::scp::cpio::client_providers::mock::MockPrivateKeyFetcherProvider;
using testing::Between;
using testing::ElementsAre;
using testing::Pointwise;
using testing::UnorderedPointwise;

constexpr std::string_view kTestAccountIdentity1 = "Test1";
constexpr std::string_view kTestAccountIdentity2 = "Test2";
constexpr std::string_view kTestAccountIdentity3 = "Test3";
constexpr std::string_view kTestGcpWipProvider1 = "Wip1";
constexpr std::string_view kTestGcpWipProvider2 = "Wip2";
constexpr std::string_view kTestGcpWipProvider3 = "Wip3";
constexpr std::string_view kTestEndpoint1 = "endpoint1";
constexpr std::string_view kTestEndpoint2 = "endpoint2";
constexpr std::string_view kTestEndpoint3 = "endpoint3";
const std::vector<std::string> kTestEndpoints{std::string{kTestEndpoint1},
                                              std::string{kTestEndpoint2},
                                              std::string{kTestEndpoint3}};
constexpr std::string_view kTestRegion1 = "region1";
constexpr std::string_view kTestRegion2 = "region2";
constexpr std::string_view kTestRegion3 = "region3";
const std::vector<std::string> kTestKeyIds = {"key_id_1", "key_id_2",
                                              "key_id_3"};
constexpr std::string_view kTestKeyIdBad = "bad_key_id";
constexpr std::string_view kTestResourceName = "encryptionKeys/key_id";
constexpr std::string_view kTestPublicKeysetHandle = "publicKeysetHandle";
constexpr std::string_view kTestPublicKeyMaterial = "publicKeyMaterial";
constexpr int kTestExpirationTime = 123456;
constexpr int kTestCreationTime = 111111;
constexpr std::string_view kTestPublicKeySignature = "publicKeySignature";
constexpr std::string_view kTestKeyEncryptionKeyUri = "keyEncryptionKeyUri";
const std::vector<std::string> kTestKeyMaterials = {
    "key-material-1", "key-material-2", "key-material-3"};
constexpr std::string_view kTestKeyMaterialBad = "bad-key-material";
constexpr std::string_view kTestPrivateKey = "Test message";
constexpr std::string_view kSinglePartyPrivateKeyJson =
    R"(
    {
    "keysetInfo": {
        "primaryKeyId": 1353288376,
        "keyInfo": [{
            "typeUrl": "type.googleapis.com/google.crypto.tink.EciesAeadHkdfPrivateKey",
            "outputPrefixType": "TINK",
            "keyId": 1353288376,
            "status": "ENABLED"
        }]
    },
    "encryptedKeyset": "AOeDD+K9avWgJPATpSkvxEVqMKG1QpWzpSgOWdaY3H8CdTuEjcRWSTwtUKNIzY62C5g4sdHiFRYbHAErW8fZB0rlAfZx6Al43G/exlWzk8CZcrqEX0r/VTFsTNdGb6zmTFqLGqmV54yqsryTazF92qILsPyNuFMxm4AfZ4hUDXmHSYZPOr9FUbYkfYeQQebeUL5GKV8dSInj4l9/xnAdyG92iVqhG5V7KxsymVAVnaj8bP7JPyM2xF1VEt8YtQemibrnBHhOtkZEzUdz88O1A4qHVYW1bb/6tCtfI4dxJrydYB3fTsdjOFYpTvhoFbQTVbSkF5IPbH8acu0Zr4UWpFKDDAlg5SMgVcsxjteBouO0zum7opp2ymN1pFllNuhIDTg0X7pp5AU+8p2wGrSVrkMEFVgWmifL+dFae6KQRvpFd9sCEz4pw7Kx6uqcVsREE8P2JgxLPctMMh021LGVE25+4fjC1vslYlCRCUziZPN8W3BP9xvORxj0y9IvChBmqBcKjT56M+5C26HXWK2U26ZR7OxLIdesLQ\u003d\u003d"
    }
    )";
constexpr std::string_view kDecryptedSinglePartyKey = "singlepartytestkey";

static void GetPrivateKeyFetchingResponse(PrivateKeyFetchingResponse& response,
                                          int key_id_index, int uri_index,
                                          size_t splits_in_key_data = 3,
                                          bool bad_key_material = false) {
  auto encryption_key = std::make_shared<EncryptionKey>();
  encryption_key->resource_name =
      std::make_shared<std::string>(kTestResourceName);
  encryption_key->expiration_time_in_ms = kTestExpirationTime;
  encryption_key->creation_time_in_ms = kTestCreationTime;
  encryption_key->encryption_key_type =
      EncryptionKeyType::kMultiPartyHybridEvenKeysplit;
  encryption_key->public_key_material =
      std::make_shared<std::string>(kTestPublicKeyMaterial);
  encryption_key->public_keyset_handle =
      std::make_shared<std::string>(kTestPublicKeysetHandle);

  for (auto i = 0; i < splits_in_key_data; ++i) {
    auto key_data = std::make_shared<KeyData>();
    key_data->key_encryption_key_uri =
        std::make_shared<std::string>(kTestKeyEncryptionKeyUri);
    if (i == uri_index) {
      key_data->key_material = std::make_shared<std::string>(
          bad_key_material ? kTestKeyMaterialBad : kTestKeyMaterials[i]);
    }

    key_data->public_key_signature =
        std::make_shared<std::string>(kTestPublicKeySignature);
    encryption_key->key_data.emplace_back(key_data);
  }
  encryption_key->key_id = std::make_shared<std::string>(
      bad_key_material ? kTestKeyIdBad : kTestKeyIds[key_id_index]);
  response.encryption_keys.emplace_back(encryption_key);
}

static absl::flat_hash_map<
    std::string, absl::flat_hash_map<std::string, PrivateKeyFetchingResponse>>
CreateSuccessKeyFetchingResponseMap(size_t splits_in_key_data = 3,
                                    size_t call_num = 3) {
  absl::flat_hash_map<
      std::string, absl::flat_hash_map<std::string, PrivateKeyFetchingResponse>>
      responses;
  for (int i = 0; i < call_num; ++i) {
    for (int j = 0; j < 3; ++j) {
      PrivateKeyFetchingResponse mock_fetching_response;
      GetPrivateKeyFetchingResponse(mock_fetching_response, i, j,
                                    splits_in_key_data);
      responses[kTestKeyIds[i]][kTestEndpoints[j]] = mock_fetching_response;
    }
  }

  return responses;
}

static absl::flat_hash_map<std::string, PrivateKeyFetchingResponse>
CreateSuccessKeyFetchingResponseMapForListByAge() {
  absl::flat_hash_map<std::string, PrivateKeyFetchingResponse> responses;
  for (int i = 0; i < 3; ++i) {
    PrivateKeyFetchingResponse mock_fetching_response;
    for (int j = 0; j < 3; ++j) {
      GetPrivateKeyFetchingResponse(mock_fetching_response, j, i);
    }
    responses[kTestEndpoints[i]] = mock_fetching_response;
  }

  return responses;
}

class PrivateKeyClientProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    PrivateKeyVendingEndpoint endpoint_1;
    endpoint_1.account_identity = kTestAccountIdentity1;
    endpoint_1.gcp_wip_provider = kTestGcpWipProvider1;
    endpoint_1.service_region = kTestRegion1;
    endpoint_1.private_key_vending_service_endpoint = kTestEndpoint1;
    PrivateKeyVendingEndpoint endpoint_2;
    endpoint_2.account_identity = kTestAccountIdentity2;
    endpoint_2.gcp_wip_provider = kTestGcpWipProvider2;
    endpoint_2.service_region = kTestRegion2;
    endpoint_2.private_key_vending_service_endpoint = kTestEndpoint2;
    PrivateKeyVendingEndpoint endpoint_3;
    endpoint_3.account_identity = kTestAccountIdentity3;
    endpoint_3.gcp_wip_provider = kTestGcpWipProvider3;
    endpoint_3.service_region = kTestRegion3;
    endpoint_3.private_key_vending_service_endpoint = kTestEndpoint3;

    PrivateKeyClientOptions private_key_client_options;
    private_key_client_options.primary_private_key_vending_endpoint =
        endpoint_1;
    private_key_client_options.secondary_private_key_vending_endpoints
        .emplace_back(endpoint_2);
    private_key_client_options.secondary_private_key_vending_endpoints
        .emplace_back(endpoint_3);

    private_key_client_provider.emplace(std::move(private_key_client_options));
    mock_private_key_fetcher =
        &private_key_client_provider->GetPrivateKeyFetcherProvider();
    mock_kms_client = &private_key_client_provider->GetKmsClientProvider();
  }

  void SetMockKmsClient(const ExecutionResult& mock_result, int8_t call_time,
                        bool mock_schedule_result = false) {
    EXPECT_CALL(*mock_kms_client, Decrypt)
        .Times(call_time)
        .WillRepeatedly(
            [=](AsyncContext<DecryptRequest, DecryptResponse>& context) {
              context.response = std::make_shared<DecryptResponse>();
              auto it = kPlaintextMap.find(context.request->ciphertext());
              if (it != kPlaintextMap.end()) {
                context.response->set_plaintext(it->second);
              }
              context.Finish(mock_result);
              if (mock_schedule_result && !mock_result.Successful()) {
                return absl::UnknownError("");
              }
              return absl::OkStatus();
            });
  }

  void SetMockPrivateKeyFetchingClient(
      const absl::flat_hash_map<
          std::string, absl::flat_hash_map<std::string, ExecutionResult>>&
          mock_results,
      const absl::flat_hash_map<
          std::string,
          absl::flat_hash_map<std::string, PrivateKeyFetchingResponse>>&
          mock_responses,
      int8_t call_time) {
    EXPECT_CALL(*mock_private_key_fetcher, FetchPrivateKey)
        .Times(call_time)
        .WillRepeatedly([=](AsyncContext<PrivateKeyFetchingRequest,
                                         PrivateKeyFetchingResponse>& context) {
          const auto& endpoint = context.request->key_vending_endpoint
                                     ->private_key_vending_service_endpoint;
          const auto& key_id = *context.request->key_id;

          const auto it = mock_results.find(key_id);
          if (it == mock_results.end()) {
            context.Finish(FailureExecutionResult(
                SC_PRIVATE_KEY_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND));
            return context.result;
          }

          const auto& endpoint_it = it->second.find(endpoint);
          if (endpoint_it == it->second.end()) {
            context.Finish(FailureExecutionResult(
                SC_PRIVATE_KEY_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND));
            return context.result;
          }

          if (endpoint_it->second.Successful()) {
            if (const auto it = mock_responses.find(key_id);
                it != mock_responses.end()) {
              if (const auto response = it->second.find(endpoint);
                  response != it->second.end()) {
                context.response = std::make_shared<PrivateKeyFetchingResponse>(
                    response->second);
              }
            }
          }
          context.Finish(endpoint_it->second);
          return SuccessExecutionResult();
        });
  }

  void SetMockPrivateKeyFetchingClientForListByAge(
      const absl::flat_hash_map<std::string, ExecutionResult>& mock_results,
      const absl::flat_hash_map<std::string, PrivateKeyFetchingResponse>&
          mock_responses,
      int8_t call_time) {
    EXPECT_CALL(*mock_private_key_fetcher, FetchPrivateKey)
        .Times(call_time)
        .WillRepeatedly([=](AsyncContext<PrivateKeyFetchingRequest,
                                         PrivateKeyFetchingResponse>& context) {
          const auto& endpoint = context.request->key_vending_endpoint
                                     ->private_key_vending_service_endpoint;
          const auto it = mock_results.find(endpoint);
          if (it == mock_results.end()) {
            context.Finish(FailureExecutionResult(
                SC_PRIVATE_KEY_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND));
            return context.result;
          }

          if (it->second.Successful()) {
            if (const auto it = mock_responses.find(endpoint);
                it != mock_responses.end()) {
              context.response =
                  std::make_shared<PrivateKeyFetchingResponse>(it->second);
            }
          }
          context.Finish(it->second);
          return context.result;
        });
  }

  std::vector<PrivateKey> BuildExpectedPrivateKeys(
      std::string_view encoded_private_key, uint16_t start_index = 0,
      uint16_t end_index = 2) {
    std::vector<PrivateKey> expected_keys(end_index - start_index + 1);
    for (auto i = start_index; i <= end_index; ++i) {
      uint16_t key_index = i - start_index;
      expected_keys[key_index].set_key_id(kTestKeyIds[i]);
      expected_keys[key_index].set_public_key(kTestPublicKeyMaterial);
      expected_keys[key_index].set_private_key(encoded_private_key);
      *expected_keys[key_index].mutable_expiration_time() =
          TimeUtil::MillisecondsToTimestamp(kTestExpirationTime);
      *expected_keys[key_index].mutable_creation_time() =
          TimeUtil::MillisecondsToTimestamp(kTestCreationTime);
    }
    return expected_keys;
  }

  std::optional<MockPrivateKeyClientProviderWithOverrides>
      private_key_client_provider;
  MockPrivateKeyFetcherProvider* mock_private_key_fetcher;
  MockKmsClientProvider* mock_kms_client;

  const absl::flat_hash_map<std::string, std::string> kPlaintextMap = {
      {kTestKeyMaterials[0], "\270G\005\364$\253\273\331\353\336\216>"},
      {kTestKeyMaterials[1], "\327\002\204 \232\377\002\330\225DB\f"},
      {kTestKeyMaterials[2], "; \362\240\2369\334r\r\373\253W"},
  };

  const absl::flat_hash_map<std::string, ExecutionResult>
      kMockSuccessKeyFetchingResultsForListByAge = {
          {std::string{kTestEndpoint1}, SuccessExecutionResult()},
          {std::string{kTestEndpoint2}, SuccessExecutionResult()},
          {std::string{kTestEndpoint3}, SuccessExecutionResult()},
      };

  const absl::flat_hash_map<std::string,
                            absl::flat_hash_map<std::string, ExecutionResult>>
      kMockSuccessKeyFetchingResults = {
          {kTestKeyIds[0],
           {{std::string{kTestEndpoint1}, SuccessExecutionResult()},
            {std::string{kTestEndpoint2}, SuccessExecutionResult()},
            {std::string{kTestEndpoint3}, SuccessExecutionResult()}}},
          {kTestKeyIds[1],
           {{std::string{kTestEndpoint1}, SuccessExecutionResult()},
            {std::string{kTestEndpoint2}, SuccessExecutionResult()},
            {std::string{kTestEndpoint3}, SuccessExecutionResult()}}},
          {kTestKeyIds[2],
           {{std::string{kTestEndpoint1}, SuccessExecutionResult()},
            {std::string{kTestEndpoint2}, SuccessExecutionResult()},
            {std::string{kTestEndpoint3}, SuccessExecutionResult()}}},
      };

  const absl::flat_hash_map<
      std::string, absl::flat_hash_map<std::string, PrivateKeyFetchingResponse>>
      kMockSuccessKeyFetchingResponses = CreateSuccessKeyFetchingResponseMap();
};

TEST_F(PrivateKeyClientProviderTest, ListPrivateKeysByIdsSuccess) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result, 9);

  SetMockPrivateKeyFetchingClient(kMockSuccessKeyFetchingResults,
                                  kMockSuccessKeyFetchingResponses, 9);
  ListPrivateKeysRequest request;
  request.add_key_ids(kTestKeyIds[0]);
  request.add_key_ids(kTestKeyIds[1]);
  request.add_key_ids(kTestKeyIds[2]);

  std::string encoded_private_key;
  Base64Encode(kTestPrivateKey, encoded_private_key);
  absl::Notification response_count;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      std::make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        auto expected_keys = BuildExpectedPrivateKeys(encoded_private_key);
        ASSERT_SUCCESS(context.result);
        EXPECT_THAT(context.response->private_keys(),
                    UnorderedPointwise(EqualsProto(), expected_keys));
        response_count.Notify();
      });

  EXPECT_TRUE(private_key_client_provider->ListPrivateKeys(context).ok());
  response_count.WaitForNotification();
}

TEST_F(PrivateKeyClientProviderTest, ListPrivateKeysByAgeSuccess) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result, 9);

  SetMockPrivateKeyFetchingClientForListByAge(
      kMockSuccessKeyFetchingResultsForListByAge,
      CreateSuccessKeyFetchingResponseMapForListByAge(), 3);
  ListPrivateKeysRequest request;
  request.set_max_age_seconds(kTestCreationTime);

  std::string encoded_private_key;
  Base64Encode(kTestPrivateKey, encoded_private_key);
  absl::Notification response_count;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      std::make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        auto expected_keys = BuildExpectedPrivateKeys(encoded_private_key);
        EXPECT_THAT(context.response->private_keys(),
                    UnorderedPointwise(EqualsProto(), expected_keys));
        EXPECT_SUCCESS(context.result);
        response_count.Notify();
      });

  EXPECT_TRUE(private_key_client_provider->ListPrivateKeys(context).ok());
  response_count.WaitForNotification();
}

TEST_F(PrivateKeyClientProviderTest, KeyListIsEmpty) {
  EXPECT_CALL(*mock_private_key_fetcher, FetchPrivateKey)
      .Times(3)
      .WillRepeatedly([=](AsyncContext<PrivateKeyFetchingRequest,
                                       PrivateKeyFetchingResponse>& context) {
        context.response = std::make_shared<PrivateKeyFetchingResponse>();
        context.Finish(SuccessExecutionResult());
        return context.result;
      });

  ListPrivateKeysRequest request;
  request.set_max_age_seconds(kTestCreationTime);
  absl::Notification response_count;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      std::make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        EXPECT_THAT(context.response->private_keys().size(), 0);
        EXPECT_SUCCESS(context.result);
        response_count.Notify();
      });

  EXPECT_TRUE(private_key_client_provider->ListPrivateKeys(context).ok());
  response_count.WaitForNotification();
}

TEST_F(PrivateKeyClientProviderTest, LastEndpointReturnEmptyList) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result, 6);

  absl::flat_hash_map<std::string, PrivateKeyFetchingResponse> responses;
  for (int i = 0; i < 2; ++i) {
    PrivateKeyFetchingResponse mock_fetching_response;
    for (int j = 0; j < 3; ++j) {
      GetPrivateKeyFetchingResponse(mock_fetching_response, j, i);
    }
    responses[kTestEndpoints[i]] = mock_fetching_response;
  }
  PrivateKeyFetchingResponse empty_response;
  responses[kTestEndpoints[2]] = empty_response;

  SetMockPrivateKeyFetchingClientForListByAge(
      kMockSuccessKeyFetchingResultsForListByAge, responses, 3);

  ListPrivateKeysRequest request;
  request.set_max_age_seconds(kTestCreationTime);

  absl::Notification response_count;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      std::make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        EXPECT_THAT(context.response->private_keys().size(), 0);
        EXPECT_SUCCESS(context.result);
        response_count.Notify();
      });

  EXPECT_TRUE(private_key_client_provider->ListPrivateKeys(context).ok());
  response_count.WaitForNotification();
}

TEST_F(PrivateKeyClientProviderTest, LastEndpointMissingKeySplit) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result, 8);

  absl::flat_hash_map<std::string, PrivateKeyFetchingResponse> responses;
  for (int i = 0; i < 2; ++i) {
    PrivateKeyFetchingResponse mock_fetching_response;
    for (int j = 0; j < 3; ++j) {
      GetPrivateKeyFetchingResponse(mock_fetching_response, j, i);
    }
    responses[kTestEndpoints[i]] = mock_fetching_response;
  }
  PrivateKeyFetchingResponse empty_response;
  for (int j = 0; j < 2; ++j) {
    GetPrivateKeyFetchingResponse(empty_response, j, 2);
  }
  responses[kTestEndpoints[2]] = empty_response;

  SetMockPrivateKeyFetchingClientForListByAge(
      kMockSuccessKeyFetchingResultsForListByAge, responses, 3);

  ListPrivateKeysRequest request;
  request.set_max_age_seconds(kTestCreationTime);

  std::string encoded_private_key;
  Base64Encode(kTestPrivateKey, encoded_private_key);
  absl::Notification response_count;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      std::make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        EXPECT_THAT(context.response->private_keys().size(), 2);
        auto expected_keys =
            BuildExpectedPrivateKeys(encoded_private_key, 0, 1);
        EXPECT_THAT(context.response->private_keys(),
                    UnorderedPointwise(EqualsProto(), expected_keys));
        EXPECT_SUCCESS(context.result);
        response_count.Notify();
      });

  EXPECT_TRUE(private_key_client_provider->ListPrivateKeys(context).ok());
  response_count.WaitForNotification();
}

TEST_F(PrivateKeyClientProviderTest, FirstEndpointMissingMultipleKeySplits) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result, 7);

  absl::flat_hash_map<std::string, PrivateKeyFetchingResponse> responses;
  for (int i = 1; i < 3; ++i) {
    PrivateKeyFetchingResponse mock_fetching_response;
    for (int j = 0; j < 3; ++j) {
      GetPrivateKeyFetchingResponse(mock_fetching_response, j, i);
    }
    responses[kTestEndpoints[i]] = mock_fetching_response;
  }
  PrivateKeyFetchingResponse empty_response;
  for (int j = 0; j < 1; ++j) {
    GetPrivateKeyFetchingResponse(empty_response, j, 0);
  }
  responses[kTestEndpoints[0]] = empty_response;

  SetMockPrivateKeyFetchingClientForListByAge(
      kMockSuccessKeyFetchingResultsForListByAge, responses, 3);

  ListPrivateKeysRequest request;
  request.set_max_age_seconds(kTestCreationTime);

  std::string encoded_private_key;
  Base64Encode(kTestPrivateKey, encoded_private_key);
  absl::Notification response_count;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      std::make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        EXPECT_THAT(context.response->private_keys().size(), 1);
        auto expected_keys =
            BuildExpectedPrivateKeys(encoded_private_key, 0, 0);
        EXPECT_THAT(context.response->private_keys(),
                    UnorderedPointwise(EqualsProto(), expected_keys));
        EXPECT_SUCCESS(context.result);
        response_count.Notify();
      });

  EXPECT_TRUE(private_key_client_provider->ListPrivateKeys(context).ok());
  response_count.WaitForNotification();
}

TEST_F(PrivateKeyClientProviderTest,
       IgnoreUnmatchedEndpointsAndKeyDataSplitsFailureForListByAge) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result, 7);

  absl::flat_hash_map<std::string, PrivateKeyFetchingResponse> responses;
  for (int i = 1; i < 3; ++i) {
    PrivateKeyFetchingResponse mock_fetching_response;
    for (int j = 0; j < 2; ++j) {
      GetPrivateKeyFetchingResponse(mock_fetching_response, j, i);
    }
    responses[kTestEndpoints[i]] = mock_fetching_response;
  }
  PrivateKeyFetchingResponse corrupted_response;
  for (int j = 0; j < 3; ++j) {
    GetPrivateKeyFetchingResponse(corrupted_response, j, 0);
  }
  corrupted_response.encryption_keys[0]->key_data.pop_back();
  responses[kTestEndpoints[0]] = corrupted_response;

  SetMockPrivateKeyFetchingClientForListByAge(
      kMockSuccessKeyFetchingResultsForListByAge, responses, 3);

  ListPrivateKeysRequest request;
  request.set_max_age_seconds(kTestCreationTime);

  std::string encoded_private_key;
  Base64Encode(kTestPrivateKey, encoded_private_key);
  absl::Notification response_count;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      std::make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        EXPECT_THAT(context.response->private_keys().size(), 1);
        auto expected_keys =
            BuildExpectedPrivateKeys(encoded_private_key, 1, 1);
        EXPECT_THAT(context.response->private_keys(),
                    UnorderedPointwise(EqualsProto(), expected_keys));
        EXPECT_SUCCESS(context.result);
        response_count.Notify();
      });

  EXPECT_TRUE(private_key_client_provider->ListPrivateKeys(context).ok());
  response_count.WaitForNotification();
}

TEST_F(PrivateKeyClientProviderTest, FetchingPrivateKeysFailed) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result, 3);

  PrivateKeyFetchingResponse mock_fetching_response;
  GetPrivateKeyFetchingResponse(mock_fetching_response, 0, 0);
  EXPECT_CALL(*mock_private_key_fetcher, FetchPrivateKey)
      .Times(6)
      .WillRepeatedly([=](AsyncContext<PrivateKeyFetchingRequest,
                                       PrivateKeyFetchingResponse>& context) {
        if (*context.request->key_id == kTestKeyIdBad) {
          context.Finish(FailureExecutionResult(SC_UNKNOWN));
          return SuccessExecutionResult();
        }

        context.response = std::make_shared<PrivateKeyFetchingResponse>(
            mock_fetching_response);
        context.Finish(SuccessExecutionResult());
        return context.result;
      });

  // One private key obtain failed in the list will return failed
  // ListPrivateKeysResponse.
  ListPrivateKeysRequest request;
  request.add_key_ids(kTestKeyIds[0]);
  request.add_key_ids(kTestKeyIdBad);

  absl::Notification response_count;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      std::make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_UNKNOWN)));
        response_count.Notify();
      });

  EXPECT_TRUE(private_key_client_provider->ListPrivateKeys(context).ok());
  response_count.WaitForNotification();
}

TEST_F(PrivateKeyClientProviderTest, KeyDataNotFound) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result, 2);
  auto mock_fetching_responses = CreateSuccessKeyFetchingResponseMap(2);
  SetMockPrivateKeyFetchingClient(kMockSuccessKeyFetchingResults,
                                  mock_fetching_responses, 9);

  ListPrivateKeysRequest request;
  request.add_key_ids(kTestKeyIds[0]);
  request.add_key_ids(kTestKeyIds[1]);
  request.add_key_ids(kTestKeyIds[2]);

  auto expected_result =
      FailureExecutionResult(SC_PRIVATE_KEY_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND);
  absl::Notification response_count;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      std::make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        EXPECT_THAT(context.result, ResultIs(expected_result));
        response_count.Notify();
      });

  EXPECT_TRUE(private_key_client_provider->ListPrivateKeys(context).ok());
  response_count.WaitForNotification();
}

TEST_F(PrivateKeyClientProviderTest,
       FailedWithUnmatchedEndpointsAndKeyDataSplits) {
  auto mock_result = SuccessExecutionResult();
  SetMockKmsClient(mock_result, 9);

  auto mock_fetching_responses = CreateSuccessKeyFetchingResponseMap(3);
  PrivateKeyFetchingResponse corrupted_response =
      mock_fetching_responses[kTestKeyIds[0]][kTestEndpoints[0]];
  corrupted_response.encryption_keys[0]->key_data.pop_back();
  mock_fetching_responses[kTestKeyIds[0]][kTestEndpoints[0]] =
      corrupted_response;

  SetMockPrivateKeyFetchingClient(kMockSuccessKeyFetchingResults,
                                  mock_fetching_responses, 9);

  ListPrivateKeysRequest request;
  request.add_key_ids(kTestKeyIds[0]);
  request.add_key_ids(kTestKeyIds[1]);
  request.add_key_ids(kTestKeyIds[2]);

  auto expected_result = FailureExecutionResult(
      SC_PRIVATE_KEY_CLIENT_PROVIDER_UNMATCHED_ENDPOINTS_SPLITS);
  absl::Notification response_count;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      std::make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        EXPECT_THAT(context.result, ResultIs(expected_result));
        response_count.Notify();
      });

  EXPECT_TRUE(private_key_client_provider->ListPrivateKeys(context).ok());
  response_count.WaitForNotification();
}

TEST_F(PrivateKeyClientProviderTest, FailedWithDecryptPrivateKey) {
  auto mock_result = FailureExecutionResult(SC_UNKNOWN);
  SetMockKmsClient(mock_result, 1, true);

  SetMockPrivateKeyFetchingClient(kMockSuccessKeyFetchingResults,
                                  kMockSuccessKeyFetchingResponses, 9);

  ListPrivateKeysRequest request;
  request.add_key_ids(kTestKeyIds[0]);
  request.add_key_ids(kTestKeyIds[1]);
  request.add_key_ids(kTestKeyIds[2]);

  absl::Notification response_count;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      std::make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        EXPECT_THAT(context.result, ResultIs(mock_result));
        response_count.Notify();
      });

  EXPECT_TRUE(private_key_client_provider->ListPrivateKeys(context).ok());
  response_count.WaitForNotification();
}

TEST_F(PrivateKeyClientProviderTest, FailedWithDecrypt) {
  auto mock_result = FailureExecutionResult(SC_UNKNOWN);
  SetMockKmsClient(mock_result, 9);

  SetMockPrivateKeyFetchingClient(kMockSuccessKeyFetchingResults,
                                  kMockSuccessKeyFetchingResponses, 9);

  ListPrivateKeysRequest request;
  request.add_key_ids(kTestKeyIds[0]);
  request.add_key_ids(kTestKeyIds[1]);
  request.add_key_ids(kTestKeyIds[2]);

  absl::Notification response_count;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      std::make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        EXPECT_THAT(context.result, ResultIs(mock_result));
        response_count.Notify();
      });

  EXPECT_TRUE(private_key_client_provider->ListPrivateKeys(context).ok());
  response_count.WaitForNotification();
}

TEST_F(PrivateKeyClientProviderTest, FailedWithOneKmsDecryptContext) {
  auto mock_result = FailureExecutionResult(SC_UNKNOWN);
  SetMockKmsClient(mock_result, 6);

  PrivateKeyFetchingResponse mock_fetching_response;
  GetPrivateKeyFetchingResponse(mock_fetching_response, 0, 0);
  PrivateKeyFetchingResponse mock_fetching_response_bad;
  GetPrivateKeyFetchingResponse(mock_fetching_response_bad, 1, 0, 3, true);

  EXPECT_CALL(*mock_private_key_fetcher, FetchPrivateKey)
      .Times(6)
      .WillRepeatedly([=](AsyncContext<PrivateKeyFetchingRequest,
                                       PrivateKeyFetchingResponse>& context) {
        if (*context.request->key_id == kTestKeyIdBad) {
          context.response = std::make_shared<PrivateKeyFetchingResponse>(
              mock_fetching_response_bad);
        } else {
          context.response = std::make_shared<PrivateKeyFetchingResponse>(
              mock_fetching_response);
        }

        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  ListPrivateKeysRequest request;
  request.add_key_ids(kTestKeyIds[0]);
  request.add_key_ids(kTestKeyIdBad);

  absl::Notification response_count;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      std::make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        EXPECT_THAT(context.result, ResultIs(mock_result));
        response_count.Notify();
      });

  EXPECT_TRUE(private_key_client_provider->ListPrivateKeys(context).ok());
  response_count.WaitForNotification();
}

class PrivateKeyClientProviderSinglePartyKeyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    PrivateKeyVendingEndpoint endpoint_1;
    endpoint_1.account_identity = kTestAccountIdentity1;
    endpoint_1.gcp_wip_provider = kTestGcpWipProvider1;
    endpoint_1.service_region = kTestRegion1;
    endpoint_1.private_key_vending_service_endpoint = kTestEndpoint1;

    PrivateKeyClientOptions private_key_client_options;
    private_key_client_options.primary_private_key_vending_endpoint =
        endpoint_1;
    private_key_client_provider_.emplace(std::move(private_key_client_options));
    mock_private_key_fetcher_ =
        &private_key_client_provider_->GetPrivateKeyFetcherProvider();
    mock_kms_client_ = &private_key_client_provider_->GetKmsClientProvider();
  }

  void SetMockKmsClient(int8_t call_time) {
    EXPECT_CALL(*mock_kms_client_, Decrypt)
        .Times(call_time)
        .WillRepeatedly(
            [=](AsyncContext<DecryptRequest, DecryptResponse>& context) {
              context.response = std::make_shared<DecryptResponse>();
              context.response->set_plaintext(kDecryptedSinglePartyKey);
              context.Finish(SuccessExecutionResult());
              return absl::OkStatus();
            });
  }

  void SetMockPrivateKeyFetchingClient(int8_t call_time,
                                       int8_t splits_in_key_data = 1) {
    EXPECT_CALL(*mock_private_key_fetcher_, FetchPrivateKey)
        .Times(call_time)
        .WillRepeatedly([=](AsyncContext<PrivateKeyFetchingRequest,
                                         PrivateKeyFetchingResponse>& context) {
          auto encryption_key = std::make_shared<EncryptionKey>();
          encryption_key->resource_name =
              std::make_shared<std::string>(kTestResourceName);
          encryption_key->expiration_time_in_ms = kTestExpirationTime;
          encryption_key->creation_time_in_ms = kTestCreationTime;
          if (context.request->key_id &&
              *context.request->key_id == kTestKeyIds[1]) {
            encryption_key->encryption_key_type =
                EncryptionKeyType::kMultiPartyHybridEvenKeysplit;
            encryption_key->key_id =
                std::make_shared<std::string>(kTestKeyIds[1]);

            for (auto i = 0; i < 2; ++i) {
              auto key_data = std::make_shared<KeyData>();
              key_data->key_encryption_key_uri =
                  std::make_shared<std::string>(kTestKeyEncryptionKeyUri);
              if (i == 0) {
                key_data->key_material =
                    std::make_shared<std::string>("keysplit");
              }

              key_data->public_key_signature =
                  std::make_shared<std::string>(kTestPublicKeySignature);
              encryption_key->key_data.emplace_back(key_data);
            }
          } else {
            encryption_key->encryption_key_type =
                EncryptionKeyType::kSinglePartyHybridKey;
            encryption_key->key_id =
                std::make_shared<std::string>(kTestKeyIds[0]);

            for (auto i = 0; i < splits_in_key_data; ++i) {
              auto key_data = std::make_shared<KeyData>();
              key_data->key_encryption_key_uri =
                  std::make_shared<std::string>(kTestKeyEncryptionKeyUri);
              key_data->key_material =
                  std::make_shared<std::string>(kSinglePartyPrivateKeyJson);

              key_data->public_key_signature =
                  std::make_shared<std::string>(kTestPublicKeySignature);
              encryption_key->key_data.emplace_back(key_data);
            }
          }
          encryption_key->public_key_material =
              std::make_shared<std::string>(kTestPublicKeyMaterial);
          encryption_key->public_keyset_handle =
              std::make_shared<std::string>(kTestPublicKeysetHandle);

          context.response = std::make_shared<PrivateKeyFetchingResponse>();
          context.response->encryption_keys.emplace_back(encryption_key);
          context.Finish(SuccessExecutionResult());
          return context.result;
        });
  }

  std::vector<PrivateKey> BuildExpectedPrivateKeys(
      std::string_view encoded_private_key) {
    std::vector<PrivateKey> expected_keys(1);
    expected_keys[0].set_key_id(kTestKeyIds[0]);
    expected_keys[0].set_public_key(kTestPublicKeyMaterial);
    expected_keys[0].set_private_key(encoded_private_key);
    *expected_keys[0].mutable_expiration_time() =
        TimeUtil::MillisecondsToTimestamp(kTestExpirationTime);
    *expected_keys[0].mutable_creation_time() =
        TimeUtil::MillisecondsToTimestamp(kTestCreationTime);
    return expected_keys;
  }

  std::optional<MockPrivateKeyClientProviderWithOverrides>
      private_key_client_provider_;
  MockPrivateKeyFetcherProvider* mock_private_key_fetcher_;
  MockKmsClientProvider* mock_kms_client_;
};

TEST_F(PrivateKeyClientProviderSinglePartyKeyTest, ListSinglePartyKeysSuccess) {
  SetMockKmsClient(1);

  SetMockPrivateKeyFetchingClient(1);

  ListPrivateKeysRequest request;
  request.add_key_ids(kTestKeyIds[0]);

  std::string encoded_private_key;
  Base64Encode(kDecryptedSinglePartyKey, encoded_private_key);
  absl::Notification response_count;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      std::make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        auto expected_keys = BuildExpectedPrivateKeys(encoded_private_key);
        EXPECT_THAT(context.response->private_keys(),
                    UnorderedPointwise(EqualsProto(), expected_keys));
        EXPECT_SUCCESS(context.result);
        response_count.Notify();
      });

  EXPECT_TRUE(private_key_client_provider_->ListPrivateKeys(context).ok());
  response_count.WaitForNotification();
}

TEST_F(PrivateKeyClientProviderSinglePartyKeyTest,
       MixedSingleAndMultiPartyPrivateKeysSuccess) {
  PrivateKeyVendingEndpoint endpoint_1;
  endpoint_1.account_identity = kTestAccountIdentity1;
  endpoint_1.gcp_wip_provider = kTestGcpWipProvider1;
  endpoint_1.service_region = kTestRegion1;
  endpoint_1.private_key_vending_service_endpoint = kTestEndpoint1;
  PrivateKeyVendingEndpoint endpoint_2;
  endpoint_2.account_identity = kTestAccountIdentity2;
  endpoint_2.gcp_wip_provider = kTestGcpWipProvider2;
  endpoint_2.service_region = kTestRegion2;
  endpoint_2.private_key_vending_service_endpoint = kTestEndpoint2;

  PrivateKeyClientOptions private_key_client_options;
  private_key_client_options.primary_private_key_vending_endpoint = endpoint_1;
  private_key_client_options.secondary_private_key_vending_endpoints
      .emplace_back(endpoint_2);

  private_key_client_provider_.emplace(std::move(private_key_client_options));
  mock_private_key_fetcher_ =
      &private_key_client_provider_->GetPrivateKeyFetcherProvider();
  mock_kms_client_ = &private_key_client_provider_->GetKmsClientProvider();

  SetMockKmsClient(4);

  SetMockPrivateKeyFetchingClient(4);

  ListPrivateKeysRequest request;
  request.add_key_ids(kTestKeyIds[0]);
  request.add_key_ids(kTestKeyIds[1]);

  std::string encoded_single_party_private_key;
  Base64Encode(kDecryptedSinglePartyKey, encoded_single_party_private_key);

  std::vector<std::byte> xor_secret =
      PrivateKeyClientUtils::StrToBytes(kDecryptedSinglePartyKey);
  std::vector<std::byte> next_piece =
      PrivateKeyClientUtils::StrToBytes(kDecryptedSinglePartyKey);
  xor_secret = PrivateKeyClientUtils::XOR(xor_secret, next_piece);
  std::string key_string(reinterpret_cast<const char*>(&xor_secret[0]),
                         xor_secret.size());
  std::string encoded_multi_party_private_key;
  Base64Encode(key_string, encoded_multi_party_private_key);
  absl::Notification response_count;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      std::make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        std::vector<PrivateKey> expected_keys(2);
        auto single_keys =
            BuildExpectedPrivateKeys(encoded_single_party_private_key);
        expected_keys[0] = single_keys[0];
        expected_keys[1].set_key_id(kTestKeyIds[1]);
        expected_keys[1].set_public_key(kTestPublicKeyMaterial);
        expected_keys[1].set_private_key(encoded_multi_party_private_key);
        *expected_keys[1].mutable_expiration_time() =
            TimeUtil::MillisecondsToTimestamp(kTestExpirationTime);
        *expected_keys[1].mutable_creation_time() =
            TimeUtil::MillisecondsToTimestamp(kTestCreationTime);

        EXPECT_THAT(context.response->private_keys(),
                    Pointwise(EqualsProto(), expected_keys));
        EXPECT_SUCCESS(context.result);
        response_count.Notify();
      });

  EXPECT_TRUE(private_key_client_provider_->ListPrivateKeys(context).ok());
  response_count.WaitForNotification();
}

TEST_F(PrivateKeyClientProviderSinglePartyKeyTest, ListSinglePartyKeysFailure) {
  SetMockPrivateKeyFetchingClient(1, 2);

  ListPrivateKeysRequest request;
  request.set_max_age_seconds(kTestCreationTime);

  absl::Notification response_count;
  AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse> context(
      std::make_shared<ListPrivateKeysRequest>(request),
      [&](AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
              context) {
        EXPECT_THAT(
            context.result,
            ResultIs(FailureExecutionResult(
                SC_PRIVATE_KEY_CLIENT_PROVIDER_INVALID_KEY_DATA_COUNT)));
        response_count.Notify();
      });

  EXPECT_TRUE(private_key_client_provider_->ListPrivateKeys(context).ok());
  response_count.WaitForNotification();
}
}  // namespace
}  // namespace google::scp::cpio::client_providers::test
