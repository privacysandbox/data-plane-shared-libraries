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

#include "src/cpio/client_providers/instance_client_provider/aws/aws_instance_client_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <tuple>

#include "src/cpio/client_providers/instance_client_provider/aws/error_codes.h"
#include "src/cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_AWS_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::IsSuccessfulAndHolds;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::AwsInstanceClientUtils;
using google::scp::cpio::client_providers::AwsResourceNameDetails;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::TestWithParam;
using ::testing::UnorderedElementsAre;
using ::testing::Values;

namespace {
constexpr std::string_view kResourceNameMock =
    "arn:aws:ec2:us-east-1:123456789012:instance/i-0e9801d129EXAMPLE";

constexpr std::string_view kResourceNameWithPathMock =
    "arn:aws:ec2:us-east-1:123456789012:instance/Dev/i-0e9801d129EXAMPLE";
constexpr std::string_view kResourceNameNoRegionMock =
    "arn:aws:ec2::123456789012:instance/i-0e9801d129EXAMPLE";
constexpr std::string_view kResourceNameNoAccountMock =
    "arn:aws:ec2:us-east-1::instance/i-0e9801d129EXAMPLE";

constexpr std::string_view kInstanceIdMock = "i-0e9801d129EXAMPLE";
constexpr std::string_view kRegionMock = "us-east-1";
constexpr std::string_view kAccountIdMock = "123456789012";
}  // namespace

namespace google::scp::cpio::client_providers::test {

TEST(AwsInstanceClientUtilsTest, GetCurrentRegionCodeSuccess) {
  MockInstanceClientProvider instance_client;
  instance_client.instance_resource_name = kResourceNameMock;

  auto region_code =
      AwsInstanceClientUtils::GetCurrentRegionCode(instance_client);
  EXPECT_THAT(*region_code, StrEq(kRegionMock));
}

TEST(AwsInstanceClientUtilsTest, GetCurrentRegionCodeFailedWithResourceName) {
  MockInstanceClientProvider instance_client;
  instance_client.get_instance_resource_name_mock = absl::UnknownError("");

  auto region_code =
      AwsInstanceClientUtils::GetCurrentRegionCode(instance_client);
  EXPECT_THAT(region_code.result(),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
}

TEST(AwsInstanceClientUtilsTest, ValidateResourceNameFormat) {
  EXPECT_THAT(
      AwsInstanceClientUtils::ValidateResourceNameFormat(kResourceNameMock),
      IsSuccessful());
}

TEST(AwsInstanceClientUtilsTest, ParseInstanceIdFromInstanceResourceName) {
  EXPECT_THAT(AwsInstanceClientUtils::ParseInstanceIdFromInstanceResourceName(
                  kResourceNameMock),
              IsSuccessfulAndHolds(kInstanceIdMock));
  EXPECT_THAT(AwsInstanceClientUtils::ParseInstanceIdFromInstanceResourceName(
                  kResourceNameWithPathMock),
              IsSuccessfulAndHolds(kInstanceIdMock));
}

TEST(AwsInstanceClientUtilsTest, ParseRegionFromResourceName) {
  EXPECT_THAT(
      AwsInstanceClientUtils::ParseRegionFromResourceName(kResourceNameMock),
      IsSuccessfulAndHolds(kRegionMock));
  EXPECT_THAT(AwsInstanceClientUtils::ParseRegionFromResourceName(
                  kResourceNameNoRegionMock),
              IsSuccessfulAndHolds(""));
}

TEST(AwsInstanceClientUtilsTest, ParseAccountIdFromResourceName) {
  EXPECT_THAT(
      AwsInstanceClientUtils::ParseAccountIdFromResourceName(kResourceNameMock),
      IsSuccessfulAndHolds(kAccountIdMock));
  EXPECT_THAT(AwsInstanceClientUtils::ParseAccountIdFromResourceName(
                  kResourceNameNoAccountMock),
              IsSuccessfulAndHolds(""));
}

TEST(AwsInstanceClientUtilsTest, GetInstanceResourceNameDetails) {
  AwsResourceNameDetails details;
  EXPECT_THAT(AwsInstanceClientUtils::GetResourceNameDetails(kResourceNameMock,
                                                             details),
              IsSuccessful());
  EXPECT_THAT(details.account_id, StrEq(kAccountIdMock));
  EXPECT_THAT(details.region, StrEq(kRegionMock));
  EXPECT_THAT(details.resource_id, StrEq(kInstanceIdMock));
}

class AwsInstanceClientUtilsTestII
    : public TestWithParam<std::tuple<std::string, ExecutionResult>> {
 protected:
  std::string GetResourceName() { return std::get<0>(GetParam()); }

  ExecutionResult GetExecutionResult() { return std::get<1>(GetParam()); }
};

TEST_P(AwsInstanceClientUtilsTestII, FailedWithBadResourceName) {
  auto resource_name = GetResourceName();
  EXPECT_THAT(AwsInstanceClientUtils::ValidateResourceNameFormat(resource_name),
              ResultIs(GetExecutionResult()));
}

INSTANTIATE_TEST_SUITE_P(
    ValidateResourceNameFormat, AwsInstanceClientUtilsTestII,
    Values(
        std::make_tuple(
            "arn:aws:ec2:us-east-1:123456789012:instance/i-0e9801d129EXAMPLE",
            SuccessExecutionResult()),
        std::make_tuple("arn:aws-cn:ec2:us-east-1:123456789012:instance/"
                        "i-0e9801d129EXAMPLE",
                        SuccessExecutionResult()),
        std::make_tuple(
            "arn:aws:ec2::123456789012:instance/i-0e9801d129EXAMPLE",
            SuccessExecutionResult()),
        std::make_tuple("arn:aws:ec2:us-east-1::instance/i-0e9801d129EXAMPLE",
                        SuccessExecutionResult()),
        std::make_tuple(
            "arn:aws::us-east-1:123456789012:instance/i-0e9801d129EXAMPLE",
            FailureExecutionResult(
                SC_AWS_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME)),
        std::make_tuple(
            "arn:aws:ec2:us-east-1:abc123456789012:instance/"
            "i-0e9801d129EXAMPLE",
            FailureExecutionResult(
                SC_AWS_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME)),
        std::make_tuple(
            "arn:aws-mess:ec2:us-east-1:123456789012:instance/"
            "i-0e9801d129EXAMPLE",
            FailureExecutionResult(
                SC_AWS_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME))));

}  // namespace google::scp::cpio::client_providers::test
