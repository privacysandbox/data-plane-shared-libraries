// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "instance_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "nlohmann/json.hpp"
#include "src/public/core/test_execution_result_matchers.h"

namespace google::scp::cpio::client_providers {
namespace {
using ::google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using ::google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using ::google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameRequest;
using ::google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameResponse;
using ::google::scp::core::AsyncContext;
using ::testing::Eq;
using ::testing::Test;
using json = nlohmann::json;

// Test environment variables.
constexpr std::string_view kTestPodResourceId = "test_pod_resource_id";
constexpr std::string_view kTestPodLabels =
    R"({"label1":"value1", "label2":"value2"})";

class KubernetesInstanceClientProviderTest : public Test {
 protected:
  void SetUp() override {
    // Set environment variables.
    setenv(kPodResourceIdEnvVar.data(), kTestPodResourceId.data(), 1);
    setenv(kPodLabelsEnvVar.data(), kTestPodLabels.data(), 1);

    provider_ = std::make_unique<KubernetesInstanceClientProvider>();
  }
  void TearDown() override {
    // Unset environment variables.
    unsetenv(kPodResourceIdEnvVar.data());
    unsetenv(kPodLabelsEnvVar.data());
  }

  std::unique_ptr<KubernetesInstanceClientProvider> provider_;
};

TEST_F(KubernetesInstanceClientProviderTest,
       GetInstanceDetailsByResourceNameReadsPodLabelsByEnvVar) {
  absl::Notification done;
  auto request = std::make_shared<GetInstanceDetailsByResourceNameRequest>();
  request->set_instance_resource_name(kTestPodResourceId);

  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          std::move(request),
          [&](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            EXPECT_THAT(context.result, core::test::IsSuccessful());
            const auto& details = context.response->instance_details();

            EXPECT_THAT(details.instance_id(), Eq(kTestPodResourceId));
            std::map<std::string, std::string> actual_labels(
                details.labels().begin(), details.labels().end());
            std::map<std::string, std::string> expected_labels = {
                {"label1", "value1"},
                {"label2", "value2"},
            };
            EXPECT_THAT(actual_labels, testing::ContainerEq(expected_labels));

            done.Notify();
          });

  EXPECT_TRUE(provider_->GetInstanceDetailsByResourceName(context).ok());
  done.WaitForNotification();
}

TEST_F(KubernetesInstanceClientProviderTest,
       GetInstanceDetailsByResourceNameInvalidLabelJsonReturnsError) {
  // Override setup with invalid JSON label string.
  setenv(kPodLabelsEnvVar.data(), R"(invalid json)", 1);

  auto request = std::make_shared<GetInstanceDetailsByResourceNameRequest>();
  request->set_instance_resource_name(kTestPodResourceId);

  AsyncContext<GetInstanceDetailsByResourceNameRequest,
               GetInstanceDetailsByResourceNameResponse>
      context(
          std::move(request),
          [](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                          GetInstanceDetailsByResourceNameResponse>& context) {
            // noop.
          });

  EXPECT_FALSE(provider_->GetInstanceDetailsByResourceName(context).ok());
}

}  // namespace
}  // namespace google::scp::cpio::client_providers
