/*
 * Portions Copyright (c) Microsoft Corporation
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

#include "src/cpio/client_providers/instance_client_provider/azure/azure_instance_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/strings/str_cat.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/notification.h"
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
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::AzureInstanceClientProvider;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

namespace {
inline constexpr char kOperatorTagName[] = "operator";
inline constexpr char kEnvironmentTagName[] = "environment";
inline constexpr char kServiceTagName[] = "service";
constexpr char kInstanceResourceName[] = "azure_instance_resource_name";
constexpr char kInstanceId[] = "azure_instance_id";
constexpr char kOperatorTagValue[] = "azure_operator";
constexpr char kEnvironmentTagValue[] = "azure_environment";
constexpr char kServiceTagValue[] = "azure_service";
}  // namespace

namespace google::scp::cpio::client_providers::test {
class AzureInstanceClientProviderTest : public testing::Test {
 protected:
  AzureInstanceClientProviderTest()
      : instance_provider_(std::make_unique<AzureInstanceClientProvider>()) {}

  ~AzureInstanceClientProviderTest() {}

  std::unique_ptr<AzureInstanceClientProvider> instance_provider_;
};

TEST_F(AzureInstanceClientProviderTest,
       GetCurrentInstanceResourceNameSyncNotImplemented) {
  std::string resource_name;

  EXPECT_FALSE(
      instance_provider_->GetCurrentInstanceResourceNameSync(resource_name)
          .ok());
}

TEST_F(AzureInstanceClientProviderTest, GetCurrentInstanceResourceName) {
  // Currently it returns a hard coded value.
  absl::Notification done;
  AsyncContext<GetCurrentInstanceResourceNameRequest,
               GetCurrentInstanceResourceNameResponse>
      context(
          std::make_shared<GetCurrentInstanceResourceNameRequest>(),
          [&](AsyncContext<GetCurrentInstanceResourceNameRequest,
                           GetCurrentInstanceResourceNameResponse>& context) {
            ASSERT_SUCCESS(context.result);
            EXPECT_THAT(context.response->instance_resource_name(),
                        StrEq(kInstanceResourceName));
            done.Notify();
          });

  EXPECT_TRUE(instance_provider_->GetCurrentInstanceResourceName(context).ok());
  done.WaitForNotification();
}

TEST_F(AzureInstanceClientProviderTest, GetInstanceDetailsSyncNotImplemented) {
  InstanceDetails details;
  EXPECT_FALSE(
      instance_provider_
          ->GetInstanceDetailsByResourceNameSync(kInstanceResourceName, details)
          .ok());
}

TEST_F(AzureInstanceClientProviderTest, GetInstanceDetailsSuccess) {
  absl::Notification done;
  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          std::make_shared<GetInstanceDetailsByResourceNameRequest>(),
          [&](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            ASSERT_SUCCESS(context.result);
            const auto& details = context.response->instance_details();
            EXPECT_THAT(details.instance_id(), StrEq(kInstanceId));
            // We need to update implementation to return networks.
            EXPECT_THAT(details.networks(), SizeIs(0));
            EXPECT_THAT(details.labels(),
                        UnorderedElementsAre(
                            Pair(kOperatorTagName, kOperatorTagValue),
                            Pair(kEnvironmentTagName, kEnvironmentTagValue),
                            Pair(kServiceTagName, kServiceTagValue)));
            done.Notify();
          });

  EXPECT_TRUE(
      instance_provider_->GetInstanceDetailsByResourceName(context).ok());
  done.WaitForNotification();
}

TEST_F(AzureInstanceClientProviderTest, GetTagsByResourceNameNotImplemented) {
  AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>
      context(std::make_shared<GetTagsByResourceNameRequest>(),
              [&](AsyncContext<GetTagsByResourceNameRequest,
                               GetTagsByResourceNameResponse>& context) {});

  EXPECT_FALSE(instance_provider_->GetTagsByResourceName(context).ok());
}

}  // namespace google::scp::cpio::client_providers::test
