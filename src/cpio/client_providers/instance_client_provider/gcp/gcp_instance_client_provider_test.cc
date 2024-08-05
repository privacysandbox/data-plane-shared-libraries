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

#include "src/cpio/client_providers/instance_client_provider/gcp/gcp_instance_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/notification.h"
#include "src/core/curl_client/mock/mock_curl_client.h"
#include "src/cpio/client_providers/auth_token_provider/mock/mock_auth_token_provider.h"
#include "src/cpio/client_providers/instance_client_provider/gcp/error_codes.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::InstanceDetails;
using google::scp::core::AsyncContext;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_INSTANCE_DETAILS_RESPONSE_MALFORMED;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_RESOURCE_TAGS_RESPONSE_MALFORMED;
using google::scp::core::errors::SC_GCP_INSTANCE_CLIENT_ZONE_PARSING_FAILURE;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::MockCurlClient;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::GcpInstanceClientProvider;
using google::scp::cpio::client_providers::mock::MockAuthTokenProvider;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

namespace {
constexpr std::string_view kURIForProjectId =
    "http://metadata.google.internal/computeMetadata/v1/project/project-id";
constexpr std::string_view kURIForInstanceId =
    "http://metadata.google.internal/computeMetadata/v1/instance/id";
constexpr std::string_view kURIForInstanceZone =
    "http://metadata.google.internal/computeMetadata/v1/instance/zone";
constexpr std::string_view kMetadataFlavorHeaderKey = "Metadata-Flavor";
constexpr std::string_view kMetadataFlavorHeaderValue = "Google";
constexpr std::string_view kProjectIdResult = "123456";
constexpr std::string_view kZoneResult = "projects/123456/zones/us-central1-c";
constexpr std::string_view kInstanceIdResult = "1234567";
constexpr std::string_view kInstanceResourceName =
    R"(//compute.googleapis.com/projects/123456/zones/us-central1-c/instances/1234567)";
constexpr std::string_view kResourceId =
    "projects/123456/zones/us-central1-c/instances/1234567";
constexpr std::string_view kZoneMock = "us-central1-c";

constexpr std::string_view kGcpInstanceGetUrlPrefix =
    "https://compute.googleapis.com/compute/v1/";
constexpr std::string_view kSessionTokenMock = "session-token-test";
constexpr std::string_view kAuthorizationHeaderKey = "Authorization";
constexpr std::string_view kBearerTokenPrefix = "Bearer ";

constexpr std::string_view kResourceManagerUriFormat =
    "https://$0cloudresourcemanager.googleapis.com/v3/tagBindings";
constexpr std::string_view kParentParameter = "parent=";
constexpr std::string_view kPageSizeSetting = "pageSize=300";

}  // namespace

namespace google::scp::cpio::client_providers::test {
class GcpInstanceClientProviderTest : public testing::Test {
 protected:
  GcpInstanceClientProviderTest()
      : instance_provider_(&authorizer_provider_, &http1_client_,
                           &http2_client_) {
    get_details_path_mock_ =
        absl::StrCat(kGcpInstanceGetUrlPrefix, kResourceId);
    get_details_request_ =
        std::make_shared<GetInstanceDetailsByResourceNameRequest>();
    get_details_request_->set_instance_resource_name(kInstanceResourceName);

    get_tag_path_mock_ = absl::Substitute(kResourceManagerUriFormat,
                                          absl::StrCat(kZoneMock, "-"));
    get_tags_request_ = std::make_shared<GetTagsByResourceNameRequest>();
    get_tags_request_->set_resource_name(kInstanceResourceName);
  }

  MockCurlClient http1_client_;
  MockCurlClient http2_client_;
  MockAuthTokenProvider authorizer_provider_;
  GcpInstanceClientProvider instance_provider_;

  std::string get_details_path_mock_;
  std::shared_ptr<GetInstanceDetailsByResourceNameRequest> get_details_request_;
  std::string get_tag_path_mock_;
  std::shared_ptr<GetTagsByResourceNameRequest> get_tags_request_;
};

TEST_F(GcpInstanceClientProviderTest, GetCurrentInstanceResourceNameSync) {
  std::string project_id_result{kProjectIdResult};
  std::string zone_result{kZoneResult};
  std::string id_result{kInstanceIdResult};

  EXPECT_CALL(http1_client_, PerformRequest)
      .Times(3)
      .WillRepeatedly([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;

        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(
            request.headers,
            Pointee(UnorderedElementsAre(
                Pair(kMetadataFlavorHeaderKey, kMetadataFlavorHeaderValue))));

        context.response = std::make_shared<HttpResponse>();
        if (*request.path == kURIForProjectId) {
          context.response->body = BytesBuffer(project_id_result);
        }
        if (*request.path == kURIForInstanceZone) {
          context.response->body = BytesBuffer(zone_result);
        }
        if (*request.path == kURIForInstanceId) {
          context.response->body = BytesBuffer(id_result);
        }
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  std::string resource_name;

  EXPECT_TRUE(
      instance_provider_.GetCurrentInstanceResourceNameSync(resource_name)
          .ok());

  EXPECT_THAT(resource_name,
              absl::StrCat("//compute.googleapis.com/", kResourceId));
}

TEST_F(GcpInstanceClientProviderTest,
       GetCurrentInstanceResourceNameSyncFailedWithHttpPerformRequest) {
  EXPECT_CALL(http1_client_, PerformRequest)
      .Times(3)
      .WillRepeatedly([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(
            request.headers,
            Pointee(UnorderedElementsAre(
                Pair(kMetadataFlavorHeaderKey, kMetadataFlavorHeaderValue))));
        context.Finish(FailureExecutionResult(SC_UNKNOWN));
        return SuccessExecutionResult();
      });

  std::string resource_name;
  EXPECT_FALSE(
      instance_provider_.GetCurrentInstanceResourceNameSync(resource_name)
          .ok());

  EXPECT_THAT(resource_name, IsEmpty());
}

TEST_F(GcpInstanceClientProviderTest, GetCurrentInstanceResourceName) {
  std::string project_id_result{kProjectIdResult};
  std::string zone_result{kZoneResult};
  std::string id_result{kInstanceIdResult};

  EXPECT_CALL(http1_client_, PerformRequest)
      .Times(3)
      .WillRepeatedly([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;

        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(
            request.headers,
            Pointee(UnorderedElementsAre(
                Pair(kMetadataFlavorHeaderKey, kMetadataFlavorHeaderValue))));

        context.response = std::make_shared<HttpResponse>();
        if (*request.path == kURIForProjectId) {
          context.response->body = BytesBuffer(project_id_result);
        }
        if (*request.path == kURIForInstanceZone) {
          context.response->body = BytesBuffer(zone_result);
        }
        if (*request.path == kURIForInstanceId) {
          context.response->body = BytesBuffer(id_result);
        }
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  absl::Notification done;
  AsyncContext<GetCurrentInstanceResourceNameRequest,
               GetCurrentInstanceResourceNameResponse>
      context(
          std::make_shared<GetCurrentInstanceResourceNameRequest>(),
          [&](AsyncContext<GetCurrentInstanceResourceNameRequest,
                           GetCurrentInstanceResourceNameResponse>& context) {
            ASSERT_SUCCESS(context.result);
            EXPECT_THAT(
                context.response->instance_resource_name(),
                StrEq(absl::StrCat("//compute.googleapis.com/", kResourceId)));
            done.Notify();
          });

  EXPECT_TRUE(instance_provider_.GetCurrentInstanceResourceName(context).ok());
  done.WaitForNotification();
}

TEST_F(GcpInstanceClientProviderTest,
       FailedToGetCurrentInstanceResourceNameOnlyGotOneResult) {
  std::string id_result{kInstanceIdResult};

  EXPECT_CALL(http1_client_, PerformRequest)
      .Times(3)
      .WillRepeatedly([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;

        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(
            request.headers,
            Pointee(UnorderedElementsAre(
                Pair(kMetadataFlavorHeaderKey, kMetadataFlavorHeaderValue))));

        context.response = std::make_shared<HttpResponse>();
        if (*request.path == kURIForProjectId) {
          context.result = FailureExecutionResult(SC_UNKNOWN);
        }
        if (*request.path == kURIForInstanceZone) {
          context.result = FailureExecutionResult(SC_UNKNOWN);
        }
        if (*request.path == kURIForInstanceId) {
          context.response->body = BytesBuffer(id_result);
          context.result = SuccessExecutionResult();
        }
        context.Finish();
        return SuccessExecutionResult();
      });

  absl::Notification done;
  AsyncContext<GetCurrentInstanceResourceNameRequest,
               GetCurrentInstanceResourceNameResponse>
      context(
          std::make_shared<GetCurrentInstanceResourceNameRequest>(),
          [&](AsyncContext<GetCurrentInstanceResourceNameRequest,
                           GetCurrentInstanceResourceNameResponse>& context) {
            EXPECT_THAT(context.result,
                        ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            done.Notify();
          });

  EXPECT_TRUE(instance_provider_.GetCurrentInstanceResourceName(context).ok());
  done.WaitForNotification();
}

TEST_F(GcpInstanceClientProviderTest, FailedToGetCurrentInstanceResourceName) {
  EXPECT_CALL(http1_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;

        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(
            request.headers,
            Pointee(UnorderedElementsAre(
                Pair(kMetadataFlavorHeaderKey, kMetadataFlavorHeaderValue))));
        return FailureExecutionResult(SC_UNKNOWN);
      });

  absl::Notification done;
  AsyncContext<GetCurrentInstanceResourceNameRequest,
               GetCurrentInstanceResourceNameResponse>
      context(
          std::make_shared<GetCurrentInstanceResourceNameRequest>(),
          [&](AsyncContext<GetCurrentInstanceResourceNameRequest,
                           GetCurrentInstanceResourceNameResponse>& context) {
            EXPECT_THAT(context.result,
                        ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            done.Notify();
          });

  EXPECT_FALSE(instance_provider_.GetCurrentInstanceResourceName(context).ok());
  done.WaitForNotification();
}

TEST_F(GcpInstanceClientProviderTest, GetInstanceDetailsSyncSuccess) {
  EXPECT_CALL(authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = std::make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            std::make_shared<std::string>(kSessionTokenMock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  // Example of Instance `get` JSON response, with some fields of no interest
  // removed.
  auto details_response_mock = R"""(
        {
          "kind": "compute#instance",
          "id": "123456789",
          "creationTimestamp": "2023-01-31T10:34:02.695-08:00",
          "name": "instance-1-test-temp",
          "description": "",
          "tags": {
            "fingerprint": "428rSM="
          },
          "machineType": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a/machineTypes/e2-medium",
          "status": "RUNNING",
          "zone": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a",
          "canIpForward": false,
          "labels": {
            "test-label-1": "test-value-1",
            "test-label-2": "test-value-2"
          },
          "networkInterfaces": [
            {
              "kind": "compute#networkInterface",
              "network": "https://www.googleapis.com/compute/v1/projects/project123/global/networks/default",
              "subnetwork": "https://www.googleapis.com/compute/v1/projects/project123/regions/us-central1/subnetworks/default",
              "networkIP": "10.10.0.99",
              "name": "nic0",
              "accessConfigs": [
                {
                  "kind": "compute#accessConfig",
                  "type": "ONE_TO_ONE_NAT",
                  "name": "External NAT",
                  "natIP": "255.255.255.01",
                  "networkTier": "PREMIUM"
                }
              ],
              "fingerprint": "Hb2S=",
              "stackType": "IPV4_ONLY"
            },
            {
              "kind": "compute#networkInterface",
              "network": "https://www.googleapis.com/compute/v1/projects/project123/global/networks/default",
              "subnetwork": "https://www.googleapis.com/compute/v1/projects/project123/regions/us-central1/subnetworks/default",
              "networkIP": "10.125.0.53",
              "name": "nic1",
              "accessConfigs": [
                {
                  "kind": "compute#accessConfig",
                  "type": "ONE_TO_ONE_NAT",
                  "name": "External NAT",
                  "natIP": "255.255.255.02",
                  "networkTier": "PREMIUM"
                }
              ],
              "fingerprint": "IR29OyjoQ88=",
              "stackType": "IPV4_ONLY"
            }
          ]
        }
      )""";

  EXPECT_CALL(http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_details_path_mock_)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.response = std::make_shared<HttpResponse>();
        context.response->body = BytesBuffer(details_response_mock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  InstanceDetails details;
  EXPECT_TRUE(
      instance_provider_
          .GetInstanceDetailsByResourceNameSync(kInstanceResourceName, details)
          .ok());
  EXPECT_EQ(details.instance_id(), "123456789");
  EXPECT_EQ(details.networks().size(), 2);
  EXPECT_EQ(details.networks(0).public_ipv4_address(), "255.255.255.01");
  EXPECT_EQ(details.networks(0).private_ipv4_address(), "10.10.0.99");
  EXPECT_EQ(details.networks(1).public_ipv4_address(), "255.255.255.02");
  EXPECT_EQ(details.networks(1).private_ipv4_address(), "10.125.0.53");

  EXPECT_THAT(details.labels(),
              UnorderedElementsAre(Pair("test-label-1", "test-value-1"),
                                   Pair("test-label-2", "test-value-2")));
}

TEST_F(GcpInstanceClientProviderTest, GetInstanceDetailsAccessConfigLoop) {
  EXPECT_CALL(authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = std::make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            std::make_shared<std::string>(kSessionTokenMock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  // Example of Instance `get` JSON response, with some fields of no interest
  // removed.
  auto details_response_mock = R"""(
        {
          "kind": "compute#instance",
          "id": "123456789",
          "creationTimestamp": "2023-01-31T10:34:02.695-08:00",
          "name": "instance-1-test-temp",
          "description": "",
          "tags": {
            "fingerprint": "428rSM="
          },
          "machineType": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a/machineTypes/e2-medium",
          "status": "RUNNING",
          "zone": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a",
          "canIpForward": false,
          "networkInterfaces": [
            {
              "kind": "compute#networkInterface",
              "network": "https://www.googleapis.com/compute/v1/projects/project123/global/networks/default",
              "subnetwork": "https://www.googleapis.com/compute/v1/projects/project123/regions/us-central1/subnetworks/default",
              "networkIP": "10.10.0.99",
              "name": "nic0",
              "accessConfigs": [
                {
                  "kind": "compute#accessConfig",
                  "type": "ONE_TO_ONE_NAT",
                  "name": "External NAT",
                  "networkTier": "PREMIUM"
                },
                {
                  "kind": "compute#accessConfig",
                  "type": "ONE_TO_ONE_NAT",
                  "name": "External NAT",
                  "natIP": "255.255.255.02",
                  "networkTier": "PREMIUM"
                }
              ],
              "fingerprint": "Hb2S=",
              "stackType": "IPV4_ONLY"
            }
          ]
        }
      )""";

  EXPECT_CALL(http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_details_path_mock_)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.response = std::make_shared<HttpResponse>();
        context.response->body = BytesBuffer(details_response_mock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  InstanceDetails details;
  EXPECT_TRUE(
      instance_provider_
          .GetInstanceDetailsByResourceNameSync(kInstanceResourceName, details)
          .ok());
  EXPECT_EQ(details.instance_id(), "123456789");
  EXPECT_EQ(details.networks().size(), 1);
  EXPECT_EQ(details.networks(0).private_ipv4_address(), "10.10.0.99");
  EXPECT_EQ(details.networks(0).public_ipv4_address(), "255.255.255.02");
}

TEST_F(GcpInstanceClientProviderTest,
       GetInstanceDetailsSyncFailedWithHttpRequest) {
  EXPECT_CALL(authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = std::make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            std::make_shared<std::string>(kSessionTokenMock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  EXPECT_CALL(http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_details_path_mock_)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.Finish(FailureExecutionResult(SC_UNKNOWN));
        return SuccessExecutionResult();
      });

  InstanceDetails details;
  EXPECT_FALSE(
      instance_provider_
          .GetInstanceDetailsByResourceNameSync(kInstanceResourceName, details)
          .ok());
}

TEST_F(GcpInstanceClientProviderTest, GetInstanceDetailsSuccess) {
  EXPECT_CALL(authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = std::make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            std::make_shared<std::string>(kSessionTokenMock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  // Example of Instance `get` JSON response, with some fields of no interest
  // removed.
  auto details_response_mock = R"""(
        {
          "kind": "compute#instance",
          "id": "123456789",
          "creationTimestamp": "2023-01-31T10:34:02.695-08:00",
          "name": "instance-1-test-temp",
          "description": "",
          "tags": {
            "fingerprint": "428rSM="
          },
          "machineType": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a/machineTypes/e2-medium",
          "status": "RUNNING",
          "zone": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a",
          "canIpForward": false,
          "networkInterfaces": [
            {
              "kind": "compute#networkInterface",
              "network": "https://www.googleapis.com/compute/v1/projects/project123/global/networks/default",
              "subnetwork": "https://www.googleapis.com/compute/v1/projects/project123/regions/us-central1/subnetworks/default",
              "networkIP": "10.10.0.99",
              "name": "nic0",
              "accessConfigs": [
                {
                  "kind": "compute#accessConfig",
                  "type": "ONE_TO_ONE_NAT",
                  "name": "External NAT",
                  "natIP": "255.255.255.01",
                  "networkTier": "PREMIUM"
                }
              ],
              "fingerprint": "Hb2S=",
              "stackType": "IPV4_ONLY"
            }
          ]
        }
      )""";

  EXPECT_CALL(http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_details_path_mock_)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.response = std::make_shared<HttpResponse>();
        context.response->body = BytesBuffer(details_response_mock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  absl::Notification done;
  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          std::move(get_details_request_),
          [&](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            ASSERT_SUCCESS(context.result);
            const auto& details = context.response->instance_details();
            EXPECT_THAT(details.instance_id(), StrEq("123456789"));
            EXPECT_THAT(details.networks(), SizeIs(1));
            EXPECT_THAT(details.networks(0).public_ipv4_address(),
                        StrEq("255.255.255.01"));
            EXPECT_THAT(details.networks(0).private_ipv4_address(),
                        StrEq("10.10.0.99"));
            done.Notify();
          });

  EXPECT_TRUE(
      instance_provider_.GetInstanceDetailsByResourceName(context).ok());
  done.WaitForNotification();
}

TEST_F(GcpInstanceClientProviderTest,
       GetInstanceDetailsSuccessWithoutPublicIP) {
  EXPECT_CALL(authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = std::make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            std::make_shared<std::string>(kSessionTokenMock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  // Example of Instance `get` JSON response without public IP, with some fields
  // of no interest removed.
  auto details_response_mock = R"""(
        {
          "kind": "compute#instance",
          "id": "123456789",
          "creationTimestamp": "2023-01-31T10:34:02.695-08:00",
          "name": "instance-1-test-temp",
          "description": "",
          "tags": {
            "fingerprint": "428rSM="
          },
          "machineType": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a/machineTypes/e2-medium",
          "status": "RUNNING",
          "zone": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a",
          "canIpForward": false,
          "networkInterfaces": [
            {
              "kind": "compute#networkInterface",
              "network": "https://www.googleapis.com/compute/v1/projects/project123/global/networks/default",
              "subnetwork": "https://www.googleapis.com/compute/v1/projects/project123/regions/us-central1/subnetworks/default",
              "networkIP": "10.10.0.99",
              "name": "nic0",
              "accessConfigs": [
                {
                  "kind": "compute#accessConfig",
                  "type": "ONE_TO_ONE_NAT",
                  "name": "External NAT",
                  "networkTier": "PREMIUM"
                }
              ],
              "fingerprint": "Hb2S=",
              "stackType": "IPV4_ONLY"
            }
          ]
        }
      )""";

  EXPECT_CALL(http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_details_path_mock_)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.response = std::make_shared<HttpResponse>();
        context.response->body = BytesBuffer(details_response_mock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  absl::Notification done;
  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          std::move(get_details_request_),
          [&](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            ASSERT_SUCCESS(context.result);
            EXPECT_EQ(context.response->instance_details().instance_id(),
                      "123456789");
            EXPECT_THAT(context.response->instance_details()
                            .networks(0)
                            .public_ipv4_address(),
                        IsEmpty());
            EXPECT_EQ(context.response->instance_details()
                          .networks(0)
                          .private_ipv4_address(),
                      "10.10.0.99");
            done.Notify();
          });

  EXPECT_TRUE(
      instance_provider_.GetInstanceDetailsByResourceName(context).ok());
  done.WaitForNotification();
}

TEST_F(GcpInstanceClientProviderTest,
       GetInstanceDetailsFailedWithInvalidInstanceResourceName) {
  EXPECT_CALL(authorizer_provider_, GetSessionToken).Times(0);
  EXPECT_CALL(http2_client_, PerformRequest).Times(0);

  auto get_details_request_bad =
      std::make_shared<GetInstanceDetailsByResourceNameRequest>();
  // Resource ID is not a valid instance resource name without a valid prefix.
  get_details_request_bad->set_instance_resource_name(kResourceId);

  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          std::move(get_details_request_bad),
          [&](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
          });

  EXPECT_FALSE(
      instance_provider_.GetInstanceDetailsByResourceName(context).ok());
}

TEST_F(GcpInstanceClientProviderTest,
       GetInstanceDetailsFailedWithCredentialSigningFailure) {
  EXPECT_CALL(authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.Finish(FailureExecutionResult(SC_UNKNOWN));
        return SuccessExecutionResult();
      });

  EXPECT_CALL(http2_client_, PerformRequest).Times(0);

  absl::Notification done;
  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          std::move(get_details_request_),
          [&](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            EXPECT_THAT(context.result,
                        ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            done.Notify();
          });

  EXPECT_TRUE(
      instance_provider_.GetInstanceDetailsByResourceName(context).ok());
  done.WaitForNotification();
}

TEST_F(GcpInstanceClientProviderTest,
       GetInstanceDetailsFailedWithHttpPerformSignedRequest) {
  EXPECT_CALL(authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = std::make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            std::make_shared<std::string>(kSessionTokenMock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  EXPECT_CALL(http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        auto request = context.request;
        EXPECT_EQ(request->method, HttpMethod::GET);
        EXPECT_THAT(request->path, Pointee(Eq(get_details_path_mock_)));
        context.Finish(FailureExecutionResult(SC_UNKNOWN));
        return SuccessExecutionResult();
      });

  absl::Notification done;
  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          std::move(get_details_request_),
          [&](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            EXPECT_THAT(context.result,
                        ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            done.Notify();
          });

  EXPECT_TRUE(
      instance_provider_.GetInstanceDetailsByResourceName(context).ok());
  done.WaitForNotification();
}

TEST_F(GcpInstanceClientProviderTest,
       GetInstanceDetailsFailedMalformedHttpResponse) {
  EXPECT_CALL(authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = std::make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            std::make_shared<std::string>(kSessionTokenMock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  // Malformed http response without `networkInterfaces` field.
  auto details_response_mock = R"""(
        {
          "kind": "compute#instance",
          "id": "123456789",
          "creationTimestamp": "2023-01-31T10:34:02.695-08:00",
          "name": "instance-1-test-temp",
          "description": "",
          "tags": {
            "fingerprint": "428rSM="
          },
          "machineType": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a/machineTypes/e2-medium",
          "status": "RUNNING",
          "zone": "https://www.googleapis.com/compute/v1/projects/project123/zones/us-central1-a",
          "canIpForward": false,
        }
      )""";

  EXPECT_CALL(http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_details_path_mock_)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.response = std::make_shared<HttpResponse>();
        context.response->body = BytesBuffer(details_response_mock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  auto failure = FailureExecutionResult(
      SC_GCP_INSTANCE_CLIENT_INSTANCE_DETAILS_RESPONSE_MALFORMED);
  absl::Notification done;
  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          std::move(get_details_request_),
          [&](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            EXPECT_THAT(context.result, ResultIs(failure));
            done.Notify();
          });

  EXPECT_TRUE(
      instance_provider_.GetInstanceDetailsByResourceName(context).ok());
  done.WaitForNotification();
}

TEST_F(GcpInstanceClientProviderTest, GetTagsByResourceNameSuccess) {
  EXPECT_CALL(authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = std::make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            std::make_shared<std::string>(kSessionTokenMock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  // Example of Instance `get` JSON response, with some fields of no interest
  // removed.
  auto tags_response_mock = R"""(
          {
            "tagBindings": [
                {
                  "name": "name_1",
                  "parent": "parent_1",
                  "tagValue": "value_1"
                },
                {
                  "name": "name_2",
                  "parent": "parent_2",
                  "tagValue": "value_2"
                },
                {
                  "name": "name_3",
                  "parent": "parent_3",
                  "tagValue": "value_3"
                }
            ],
            "nextPageToken": ""
          }
        )""";

  EXPECT_CALL(http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_tag_path_mock_)));
        EXPECT_THAT(request.query, Pointee(absl::StrCat(
                                       kParentParameter, kInstanceResourceName,
                                       "&", kPageSizeSetting)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.response = std::make_shared<HttpResponse>();
        context.response->body = BytesBuffer(tags_response_mock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  absl::Notification done;
  AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>
      context(std::move(get_tags_request_),
              [&](AsyncContext<GetTagsByResourceNameRequest,
                               GetTagsByResourceNameResponse>& context) {
                ASSERT_SUCCESS(context.result);
                EXPECT_THAT(context.response->tags(),
                            UnorderedElementsAre(Pair("name_1", "value_1"),
                                                 Pair("name_2", "value_2"),
                                                 Pair("name_3", "value_3")));
                done.Notify();
              });

  EXPECT_TRUE(instance_provider_.GetTagsByResourceName(context).ok());
  done.WaitForNotification();
}

TEST_F(GcpInstanceClientProviderTest,
       GetTagsByResourceNameFailedWithCredentialSigningFailure) {
  EXPECT_CALL(authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.Finish(FailureExecutionResult(SC_UNKNOWN));
        return SuccessExecutionResult();
      });

  EXPECT_CALL(http2_client_, PerformRequest).Times(0);

  absl::Notification done;
  AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>
      context(std::move(get_tags_request_),
              [&](AsyncContext<GetTagsByResourceNameRequest,
                               GetTagsByResourceNameResponse>& context) {
                EXPECT_THAT(context.result,
                            ResultIs(FailureExecutionResult(SC_UNKNOWN)));
                done.Notify();
              });

  EXPECT_TRUE(instance_provider_.GetTagsByResourceName(context).ok());
  done.WaitForNotification();
}

TEST_F(GcpInstanceClientProviderTest,
       GetTagsByResourceNameFailedWithHttpPerformSignedRequest) {
  EXPECT_CALL(authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = std::make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            std::make_shared<std::string>(kSessionTokenMock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  EXPECT_CALL(http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        auto request = context.request;
        EXPECT_EQ(request->method, HttpMethod::GET);
        EXPECT_THAT(request->path, Pointee(Eq(get_tag_path_mock_)));
        context.Finish(FailureExecutionResult(SC_UNKNOWN));
        return SuccessExecutionResult();
      });

  absl::Notification done;
  AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>
      context(std::move(get_tags_request_),
              [&](AsyncContext<GetTagsByResourceNameRequest,
                               GetTagsByResourceNameResponse>& context) {
                EXPECT_THAT(context.result,
                            ResultIs(FailureExecutionResult(SC_UNKNOWN)));
                done.Notify();
              });

  EXPECT_TRUE(instance_provider_.GetTagsByResourceName(context).ok());
  done.WaitForNotification();
}

TEST_F(GcpInstanceClientProviderTest,
       GetTagsByResourceNameFailedMalformedHttpResponse) {
  EXPECT_CALL(authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = std::make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            std::make_shared<std::string>(kSessionTokenMock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  // Malformed http response in which one tagBinding missing 'tagValue'
  auto tags_response_mock = R"""(
          {
            "tagBindings": [
                {
                  "name": "name_1",
                  "parent": "parent_1",
                },
                {
                  "name": "name_2",
                  "parent": "parent_2",
                  "tagValue": "value_2"
                },
                {
                  "name": "name_3",
                  "parent": "parent_3",
                  "tagValue": "value_3"
                }
            ],
            "nextPageToken": ""
          }
        )""";

  EXPECT_CALL(http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_tag_path_mock_)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.response = std::make_shared<HttpResponse>();
        context.response->body = BytesBuffer(tags_response_mock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  absl::Notification done;
  AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>
      context(
          std::move(get_tags_request_),
          [&](AsyncContext<GetTagsByResourceNameRequest,
                           GetTagsByResourceNameResponse>& context) {
            EXPECT_THAT(
                context.result,
                ResultIs(FailureExecutionResult(
                    SC_GCP_INSTANCE_CLIENT_RESOURCE_TAGS_RESPONSE_MALFORMED)));
            done.Notify();
          });

  EXPECT_TRUE(instance_provider_.GetTagsByResourceName(context).ok());
  done.WaitForNotification();
}

TEST_F(GcpInstanceClientProviderTest,
       GetTagsByResourceNameFailedWithEmptyHttpResponse) {
  EXPECT_CALL(authorizer_provider_, GetSessionToken)
      .WillOnce([=](AsyncContext<GetSessionTokenRequest,
                                 GetSessionTokenResponse>& context) {
        context.response = std::make_shared<GetSessionTokenResponse>();
        context.response->session_token =
            std::make_shared<std::string>(kSessionTokenMock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  // Empty http response
  auto tags_response_mock = R"({})";

  EXPECT_CALL(http2_client_, PerformRequest)
      .WillOnce([=](AsyncContext<HttpRequest, HttpResponse>& context) {
        const auto& request = *context.request;
        EXPECT_EQ(request.method, HttpMethod::GET);
        EXPECT_THAT(request.path, Pointee(Eq(get_tag_path_mock_)));
        EXPECT_THAT(request.headers,
                    Pointee(UnorderedElementsAre(Pair(
                        kAuthorizationHeaderKey,
                        absl::StrCat(kBearerTokenPrefix, kSessionTokenMock)))));
        context.response = std::make_shared<HttpResponse>();
        context.response->body = BytesBuffer(tags_response_mock);
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  absl::Notification done;
  AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>
      context(std::move(get_tags_request_),
              [&](AsyncContext<GetTagsByResourceNameRequest,
                               GetTagsByResourceNameResponse>& context) {
                ASSERT_SUCCESS(context.result);
                EXPECT_THAT(context.response->tags(), IsEmpty());
                done.Notify();
              });

  EXPECT_TRUE(instance_provider_.GetTagsByResourceName(context).ok());
  done.WaitForNotification();
}

}  // namespace google::scp::cpio::client_providers::test
