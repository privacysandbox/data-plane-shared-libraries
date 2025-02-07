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

#include "src/cpio/client_providers/instance_client_provider/gcp/gcp_instance_client_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <tuple>

#include "absl/strings/substitute.h"
#include "src/cpio/client_providers/instance_client_provider/gcp/error_codes.h"
#include "src/cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::GcpInstanceClientUtils;
using google::scp::cpio::client_providers::GcpInstanceResourceNameDetails;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using testing::_;
using testing::DoAll;
using testing::Pair;
using testing::Pointee;
using testing::Return;
using testing::SetArgPointee;
using testing::TestWithParam;
using testing::UnorderedElementsAre;

namespace {
constexpr std::string_view kResourceNameMock =
    R"(//compute.googleapis.com/projects/123456789/zones/us-central1-c/instances/987654321)";

constexpr std::string_view kInstanceIdMock = "987654321";
constexpr std::string_view kZoneIdMock = "us-central1-c";
constexpr std::string_view kProjectIdMock = "123456789";

constexpr std::string_view kResourceManagerUriFormat =
    "https://$0cloudresourcemanager.googleapis.com/v3/tagBindings";
}  // namespace

namespace google::scp::cpio::client_providers::test {
TEST(GcpInstanceClientUtilsTest, GetCurrentProjectIdSuccess) {
  MockInstanceClientProvider instance_client;
  instance_client.instance_resource_name = kResourceNameMock;

  auto project_id =
      GcpInstanceClientUtils::GetCurrentProjectId(instance_client);
  EXPECT_EQ(*project_id, kProjectIdMock);
}

TEST(GcpInstanceClientUtilsTest, GetCurrentProjectIdFailedWithResourceName) {
  MockInstanceClientProvider instance_client;
  instance_client.get_instance_resource_name_mock = absl::UnknownError("");

  auto project_id =
      GcpInstanceClientUtils::GetCurrentProjectId(instance_client);
  EXPECT_THAT(project_id.result(),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
}

TEST(GcpInstanceClientUtilsTest,
     GetCurrentProjectIdFailedWithInvalidProjectId) {
  MockInstanceClientProvider instance_client;
  instance_client.instance_resource_name =
      R"(//compute.googleapis.com/projects//zones/us-central1-c/instances/987654321)";

  auto project_id =
      GcpInstanceClientUtils::GetCurrentProjectId(instance_client);
  EXPECT_THAT(project_id.result(),
              ResultIs(FailureExecutionResult(
                  SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME)));
}

TEST(GcpInstanceClientUtilsTest, ValidateInstanceResourceNameFormat) {
  EXPECT_THAT(GcpInstanceClientUtils::ValidateInstanceResourceNameFormat(
                  kResourceNameMock),
              IsSuccessful());
}

TEST(GcpInstanceClientUtilsTest, ParseInstanceIdFromInstanceResourceName) {
  auto instance_id =
      GcpInstanceClientUtils::ParseInstanceIdFromInstanceResourceName(
          kResourceNameMock);
  EXPECT_EQ(*instance_id, kInstanceIdMock);
}

TEST(GcpInstanceClientUtilsTest, ParseZoneIdFromInstanceResourceName) {
  auto zone_id = GcpInstanceClientUtils::ParseZoneIdFromInstanceResourceName(
      kResourceNameMock);
  EXPECT_EQ(*zone_id, kZoneIdMock);
}

TEST(GcpInstanceClientUtilsTest, ParseProjectIdFromInstanceResourceName) {
  auto project_id =
      GcpInstanceClientUtils::ParseProjectIdFromInstanceResourceName(
          kResourceNameMock);
  EXPECT_EQ(*project_id, kProjectIdMock);
}

TEST(GcpInstanceClientUtilsTest, GetInstanceResourceNameDetails) {
  GcpInstanceResourceNameDetails details;
  EXPECT_THAT(GcpInstanceClientUtils::GetInstanceResourceNameDetails(
                  kResourceNameMock, details),
              IsSuccessful());
  EXPECT_EQ(details.project_id, kProjectIdMock);
  EXPECT_EQ(details.zone_id, kZoneIdMock);
  EXPECT_EQ(details.instance_id, kInstanceIdMock);
}

class GcpInstanceClientUtilsTestII : public TestWithParam<std::string> {
 protected:
  std::string GetInstanceResourceName() { return GetParam(); }
};

TEST_P(GcpInstanceClientUtilsTestII, FailedWithBadInstanceResourceName) {
  auto resource_name = GetInstanceResourceName();
  EXPECT_THAT(
      GcpInstanceClientUtils::ValidateInstanceResourceNameFormat(resource_name),
      ResultIs(FailureExecutionResult(
          SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME)));
}

INSTANTIATE_TEST_SUITE_P(
    ValidateInstanceResourceNameFormat, GcpInstanceClientUtilsTestII,
    testing::Values(
        R"("projects/123456/zones/us-west1/instances/123")",
        R"("//compute.googleapis.com/projects/123456/zones/us-west1instances/123")",
        R"("//compute.googleapis.com/projects/-123456/zones/us-west1/instances/123")",
        R"("//compute.googleapis.com/projects/a123456/zones/12us-west1/instances/123")",
        R"("//compute.googleapis.com/projects/123456/zones/us-west1/instances/abc")",
        R"("//compute.googleapis.com/projects//zones/us-west1/instances/123")",
        R"("//compute.googleapis.com/projects/123456/zones//instances/123")",
        R"("//compute.googleapis.com/projects/123456/zones/us-west1/instances")",
        R"("//compute.googleapis.com/12345/zones/us-west1/instances/123")",
        R"("//compute.googleapis.com/projects/123456/us-west1/instances/123")",
        R"("//compute.googleapis.com/projects/123456/zones/us-west1/12345")"));

class GcpInstanceClientUtilsTestIII
    : public TestWithParam<std::tuple<std::string, std::string>> {
 protected:
  std::string GetResourceName() { return std::get<0>(GetParam()); }

  std::string GetResourceLocation() { return std::get<1>(GetParam()); }
};

TEST_P(GcpInstanceClientUtilsTestIII, CreateRMListTagsUrl) {
  auto resource_name = GetResourceName();

  auto path =
      absl::Substitute(kResourceManagerUriFormat, GetResourceLocation());
  EXPECT_EQ(GcpInstanceClientUtils::CreateRMListTagsUrl(resource_name), path);
}

INSTANTIATE_TEST_SUITE_P(
    RMListTagsUrl, GcpInstanceClientUtilsTestIII,
    testing::Values(
        std::make_tuple(
            R"("//run.googleapis.com/projects/PROJECT_ID/locations/LOCATION_ID/services/"
                "SERVICE_ID")",
            "LOCATION_ID-"),
        std::make_tuple(
            R"("//compute.googleapis.com/projects/PROJECT_ID/zones/ZONE/instances/"
                "INSTANCE_ID")",
            "ZONE-"),
        std::make_tuple(
            R"("//compute.googleapis.com/projects/PROJECT_ID/regions/REGION/subnetworks/"
                "SUBNETWORK")",
            "REGION-"),
        std::make_tuple(R"(BAD_INPUT/regions)", ""),
        std::make_tuple(
            R"("//iam.googleapis.com/projects/PROJECT_ID/serviceAccounts/"
                "SERVICE_ACCOUNT_EMAIL")",
            "")));

}  // namespace google::scp::cpio::client_providers::test
