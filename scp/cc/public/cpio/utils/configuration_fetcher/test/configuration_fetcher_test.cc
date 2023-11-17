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

#include "public/cpio/utils/configuration_fetcher/src/configuration_fetcher.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "core/interface/async_context.h"
#include "cpio/server/interface/configuration_keys.h"
#include "cpio/server/interface/crypto_service/configuration_keys.h"
#include "cpio/server/interface/queue_service/configuration_keys.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/mock/instance_client/mock_instance_client.h"
#include "public/cpio/mock/parameter_client/mock_parameter_client.h"
#include "public/cpio/proto/crypto_service/v1/crypto_service.pb.h"
#include "public/cpio/proto/instance_service/v1/instance_service.pb.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"
#include "public/cpio/utils/configuration_fetcher/interface/configuration_keys.h"
#include "public/cpio/utils/configuration_fetcher/src/error_codes.h"

using google::cmrt::sdk::crypto_service::v1::HpkeAead;
using google::cmrt::sdk::crypto_service::v1::HpkeKdf;
using google::cmrt::sdk::crypto_service::v1::HpkeKem;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameResponse;
using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_CONFIGURATION_FETCHER_CONVERSION_FAILED;
using google::scp::core::errors::
    SC_CONFIGURATION_FETCHER_ENVIRONMENT_NAME_NOT_FOUND;
using google::scp::core::test::IsSuccessfulAndHolds;
using google::scp::core::test::ResultIs;

namespace {
constexpr char kInstanceResourceName[] =
    "projects/123/zones/us-central-1/instances/345";
constexpr char kEnvNameTag[] = "environment-name";
constexpr char kEnvName[] = "test";
constexpr char kTestQueue[] = "test-queue";
constexpr char kTestSharedThreadCount[] = "10";
constexpr char kTestSharedThreadPoolQueueCap[] = "10000";
constexpr char kTestHpkeKem[] = "DHKEM_X25519_HKDF_SHA256";
constexpr char kTestHpkeKdf[] = "HKDF_SHA256";
constexpr char kTestHpkeAead[] = "CHACHA20_POLY1305";
constexpr char kTestLogOption[] = "ConsoleLog";
}  // namespace

namespace google::scp::cpio {
class ConfigurationFetcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_instance_client_ = std::make_unique<MockInstanceClient>();
    mock_parameter_client_ = std::make_unique<MockParameterClient>();

    fetcher_ = std::make_unique<ConfigurationFetcher>(
        mock_instance_client_.get(), mock_parameter_client_.get());
  }

  void ExpectGetCurrentInstanceResourceName(const ExecutionResult& result) {
    EXPECT_CALL(*mock_instance_client_, GetCurrentInstanceResourceName)
        .WillOnce(
            [result](
                GetCurrentInstanceResourceNameRequest request,
                Callback<GetCurrentInstanceResourceNameResponse> callback) {
              GetCurrentInstanceResourceNameResponse response;
              if (result.Successful()) {
                response.set_instance_resource_name(kInstanceResourceName);
              }
              callback(result, std::move(response));
              return result;
            });
  }

  void ExpectGetInstanceDetails(const ExecutionResult& result,
                                const std::string& tag) {
    EXPECT_CALL(*mock_instance_client_, GetInstanceDetailsByResourceName)
        .WillOnce(
            [&tag, result](
                GetInstanceDetailsByResourceNameRequest request,
                Callback<GetInstanceDetailsByResourceNameResponse> callback) {
              GetInstanceDetailsByResourceNameResponse response;
              if (result.Successful() &&
                  request.instance_resource_name() == kInstanceResourceName) {
                auto& labels =
                    *response.mutable_instance_details()->mutable_labels();
                labels[tag] = kEnvName;
              }
              callback(result, std::move(response));
              return result;
            });
  }

  void ExpectGetParameter(const ExecutionResult& result,
                          const std::string& parameter_name,
                          const std::string& parameter_value) {
    EXPECT_CALL(*mock_parameter_client_, GetParameter)
        .WillOnce([result, parameter_name, parameter_value](
                      GetParameterRequest request,
                      Callback<GetParameterResponse> callback) {
          GetParameterResponse response;
          if (result.Successful() &&
              request.parameter_name() ==
                  absl::StrCat("scp-", kEnvName, "-", parameter_name)) {
            response.set_parameter_value(parameter_value);
          }
          callback(result, std::move(response));
          return result;
        });
  }

  std::unique_ptr<MockInstanceClient> mock_instance_client_;
  std::unique_ptr<MockParameterClient> mock_parameter_client_;
  std::unique_ptr<ConfigurationFetcher> fetcher_;
  std::string env_name_tag_ = std::string(kEnvNameTag);
};

TEST_F(ConfigurationFetcherTest, GetParameterByNameAsyncSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kQueueClientQueueName,
                     kTestQueue);
  absl::Notification finished;
  auto get_context = AsyncContext<std::string, std::string>(
      std::make_shared<std::string>(kQueueClientQueueName),
      [&finished](AsyncContext<std::string, std::string> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, kTestQueue);
        finished.Notify();
      });
  EXPECT_SUCCESS(fetcher_->GetParameterByNameAsync(get_context));
  finished.WaitForNotification();
}

TEST_F(ConfigurationFetcherTest, GetParameterByNameSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kQueueClientQueueName,
                     kTestQueue);
  EXPECT_THAT(fetcher_->GetParameterByName(kQueueClientQueueName),
              IsSuccessfulAndHolds(kTestQueue));
}

TEST_F(ConfigurationFetcherTest, GetSharedLogOptionAsyncSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kSdkClientLogOption,
                     kTestLogOption);
  absl::Notification finished;
  auto get_context = AsyncContext<GetConfigurationRequest, LogOption>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, LogOption> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, LogOption::kConsoleLog);
        finished.Notify();
      });
  EXPECT_SUCCESS(fetcher_->GetSharedLogOptionAsync(get_context));
  finished.WaitForNotification();
}

TEST_F(ConfigurationFetcherTest, GetSharedLogOptionSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kSdkClientLogOption,
                     kTestLogOption);
  EXPECT_THAT(fetcher_->GetSharedLogOption(GetConfigurationRequest()),
              IsSuccessfulAndHolds(LogOption::kConsoleLog));
}

TEST_F(ConfigurationFetcherTest, GetSharedCpuThreadCountAsyncSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kSharedCpuThreadCount,
                     kTestSharedThreadCount);
  absl::Notification finished;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, 10);
        finished.Notify();
      });
  EXPECT_SUCCESS(fetcher_->GetSharedCpuThreadCountAsync(get_context));
  finished.WaitForNotification();
}

TEST_F(ConfigurationFetcherTest, GetSharedCpuThreadCountSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kSharedCpuThreadCount,
                     kTestSharedThreadCount);
  EXPECT_THAT(fetcher_->GetSharedCpuThreadCount(GetConfigurationRequest()),
              IsSuccessfulAndHolds(10));
}

TEST_F(ConfigurationFetcherTest, GetSharedCpuThreadCountAsyncExceedingMin1) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kSharedCpuThreadCount, "-10");
  absl::Notification finished;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_CONFIGURATION_FETCHER_CONVERSION_FAILED)));
        finished.Notify();
      });
  EXPECT_SUCCESS(fetcher_->GetSharedCpuThreadCountAsync(get_context));
  finished.WaitForNotification();
}

TEST_F(ConfigurationFetcherTest, GetSharedCpuThreadCountAsyncExceedingMin2) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kSharedCpuThreadCount, "-1");
  absl::Notification finished;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_CONFIGURATION_FETCHER_CONVERSION_FAILED)));
        finished.Notify();
      });
  EXPECT_SUCCESS(fetcher_->GetSharedCpuThreadCountAsync(get_context));
  finished.WaitForNotification();
}

TEST_F(ConfigurationFetcherTest, GetSharedCpuThreadCountExceedingMax) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kSharedCpuThreadCount,
                     "18446744073709551616");  // Exceeding uint64_t
  EXPECT_THAT(fetcher_->GetSharedCpuThreadCount(GetConfigurationRequest()),
              ResultIs(FailureExecutionResult(
                  SC_CONFIGURATION_FETCHER_CONVERSION_FAILED)));
}

TEST_F(ConfigurationFetcherTest, GetSharedCpuThreadPoolQueueCapAsyncSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kSharedCpuThreadPoolQueueCap,
                     kTestSharedThreadPoolQueueCap);
  absl::Notification finished;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, 10000);
        finished.Notify();
      });
  EXPECT_SUCCESS(fetcher_->GetSharedCpuThreadPoolQueueCapAsync(get_context));
  finished.WaitForNotification();
}

TEST_F(ConfigurationFetcherTest, GetSharedCpuThreadPoolQueueCapSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kSharedCpuThreadPoolQueueCap,
                     kTestSharedThreadPoolQueueCap);
  EXPECT_THAT(
      fetcher_->GetSharedCpuThreadPoolQueueCap(GetConfigurationRequest()),
      IsSuccessfulAndHolds(10000));
}

TEST_F(ConfigurationFetcherTest, GetSharedIoThreadCountAsyncSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kSharedIoThreadCount,
                     kTestSharedThreadCount);
  absl::Notification finished;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, 10);
        finished.Notify();
      });
  EXPECT_SUCCESS(fetcher_->GetSharedIoThreadCountAsync(get_context));
  finished.WaitForNotification();
}

TEST_F(ConfigurationFetcherTest, GetSharedIoThreadCountSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kSharedIoThreadCount,
                     kTestSharedThreadCount);
  EXPECT_THAT(fetcher_->GetSharedIoThreadCount(GetConfigurationRequest()),
              IsSuccessfulAndHolds(10));
}

TEST_F(ConfigurationFetcherTest, GetSharedIoThreadPoolQueueCapAsyncSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kSharedIoThreadPoolQueueCap,
                     kTestSharedThreadPoolQueueCap);
  absl::Notification finished;
  auto get_context = AsyncContext<GetConfigurationRequest, size_t>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, size_t> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, 10000);
        finished.Notify();
      });
  EXPECT_SUCCESS(fetcher_->GetSharedIoThreadPoolQueueCapAsync(get_context));
  finished.WaitForNotification();
}

TEST_F(ConfigurationFetcherTest, GetSharedIoThreadPoolQueueCapSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kSharedIoThreadPoolQueueCap,
                     kTestSharedThreadPoolQueueCap);
  EXPECT_THAT(
      fetcher_->GetSharedIoThreadPoolQueueCap(GetConfigurationRequest()),
      IsSuccessfulAndHolds(10000));
}

TEST_F(ConfigurationFetcherTest, GetQueueClientQueueNameAsyncSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kQueueClientQueueName,
                     kTestQueue);
  absl::Notification finished;
  auto get_context = AsyncContext<GetConfigurationRequest, std::string>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, std::string> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, kTestQueue);
        finished.Notify();
      });
  EXPECT_SUCCESS(fetcher_->GetQueueClientQueueNameAsync(get_context));
  finished.WaitForNotification();
}

TEST_F(ConfigurationFetcherTest, GetQueueClientQueueNameSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kQueueClientQueueName,
                     kTestQueue);
  EXPECT_THAT(fetcher_->GetQueueClientQueueName(GetConfigurationRequest()),
              IsSuccessfulAndHolds(kTestQueue));
}

TEST_F(ConfigurationFetcherTest, GetCryptoClientHpkeKemAsyncSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kCryptoClientHpkeKem,
                     kTestHpkeKem);
  absl::Notification finished;
  auto get_context = AsyncContext<GetConfigurationRequest, HpkeKem>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, HpkeKem> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, HpkeKem::DHKEM_X25519_HKDF_SHA256);
        finished.Notify();
      });
  EXPECT_SUCCESS(fetcher_->GetCryptoClientHpkeKemAsync(get_context));
  finished.WaitForNotification();
}

TEST_F(ConfigurationFetcherTest, GetCryptoClientHpkeKemSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kCryptoClientHpkeKem,
                     kTestHpkeKem);
  EXPECT_THAT(fetcher_->GetCryptoClientHpkeKem(GetConfigurationRequest()),
              IsSuccessfulAndHolds(HpkeKem::DHKEM_X25519_HKDF_SHA256));
}

TEST_F(ConfigurationFetcherTest, GetCryptoClientHpkeKemFailedToConvert) {
  auto failure =
      FailureExecutionResult(SC_CONFIGURATION_FETCHER_CONVERSION_FAILED);
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kCryptoClientHpkeKem, "unknown");
  EXPECT_THAT(fetcher_->GetCryptoClientHpkeKem(GetConfigurationRequest()),
              ResultIs(failure));
}

TEST_F(ConfigurationFetcherTest, GetCryptoClientHpkeKdfAsyncSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kCryptoClientHpkeKdf,
                     kTestHpkeKdf);
  absl::Notification finished;
  auto get_context = AsyncContext<GetConfigurationRequest, HpkeKdf>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, HpkeKdf> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, HpkeKdf::HKDF_SHA256);
        finished.Notify();
      });
  EXPECT_SUCCESS(fetcher_->GetCryptoClientHpkeKdfAsync(get_context));
  finished.WaitForNotification();
}

TEST_F(ConfigurationFetcherTest, GetCryptoClientHpkeKdfSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kCryptoClientHpkeKdf,
                     kTestHpkeKdf);
  EXPECT_THAT(fetcher_->GetCryptoClientHpkeKdf(GetConfigurationRequest()),
              IsSuccessfulAndHolds(HpkeKdf::HKDF_SHA256));
}

TEST_F(ConfigurationFetcherTest, GetCryptoClientHpkeAeadAsyncSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kCryptoClientHpkeAead,
                     kTestHpkeAead);
  absl::Notification finished;
  auto get_context = AsyncContext<GetConfigurationRequest, HpkeAead>(
      nullptr,
      [&finished](AsyncContext<GetConfigurationRequest, HpkeAead> context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(*context.response, HpkeAead::CHACHA20_POLY1305);
        finished.Notify();
      });
  EXPECT_SUCCESS(fetcher_->GetCryptoClientHpkeAeadAsync(get_context));
  finished.WaitForNotification();
}

TEST_F(ConfigurationFetcherTest, GetCryptoClientHpkeAeadSucceeded) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(SuccessExecutionResult(), kCryptoClientHpkeAead,
                     kTestHpkeAead);
  EXPECT_THAT(fetcher_->GetCryptoClientHpkeAead(GetConfigurationRequest()),
              IsSuccessfulAndHolds(HpkeAead::CHACHA20_POLY1305));
}

TEST_F(ConfigurationFetcherTest, FailedToGetCurrentInstance) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  ExpectGetCurrentInstanceResourceName(failure);
  absl::Notification finished;
  auto get_context = AsyncContext<GetConfigurationRequest, std::string>(
      nullptr, [&](AsyncContext<GetConfigurationRequest, std::string> context) {
        EXPECT_THAT(context.result, ResultIs(failure));
        finished.Notify();
      });
  EXPECT_THAT(fetcher_->GetQueueClientQueueNameAsync(get_context),
              ResultIs(failure));
  finished.WaitForNotification();
}

TEST_F(ConfigurationFetcherTest, FailedToGetInstanceDetails) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(failure, "");
  absl::Notification finished;
  auto get_context = AsyncContext<GetConfigurationRequest, std::string>(
      nullptr, [&](AsyncContext<GetConfigurationRequest, std::string> context) {
        EXPECT_THAT(context.result, ResultIs(failure));
        finished.Notify();
      });
  EXPECT_SUCCESS(fetcher_->GetQueueClientQueueNameAsync(get_context));
  finished.WaitForNotification();
}

TEST_F(ConfigurationFetcherTest, FailedToGetParameter) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), env_name_tag_);
  ExpectGetParameter(failure, kQueueClientQueueName, kTestQueue);
  absl::Notification finished;
  auto get_context = AsyncContext<GetConfigurationRequest, std::string>(
      nullptr, [&](AsyncContext<GetConfigurationRequest, std::string> context) {
        EXPECT_THAT(context.result, ResultIs(failure));
        finished.Notify();
      });
  EXPECT_SUCCESS(fetcher_->GetQueueClientQueueNameAsync(get_context));
  finished.WaitForNotification();
}

TEST_F(ConfigurationFetcherTest, EnvNameNotFound) {
  ExpectGetCurrentInstanceResourceName(SuccessExecutionResult());
  ExpectGetInstanceDetails(SuccessExecutionResult(), "invalid_tag");
  absl::Notification finished;
  auto get_context = AsyncContext<GetConfigurationRequest, std::string>(
      nullptr, [&](AsyncContext<GetConfigurationRequest, std::string> context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_CONFIGURATION_FETCHER_ENVIRONMENT_NAME_NOT_FOUND)));
        finished.Notify();
      });
  EXPECT_SUCCESS(fetcher_->GetQueueClientQueueNameAsync(get_context));
  finished.WaitForNotification();
}
}  // namespace google::scp::cpio
