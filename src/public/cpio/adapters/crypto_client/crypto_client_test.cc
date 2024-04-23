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
// WITHOUT WARRANTIES OR finishedS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/public/cpio/adapters/crypto_client/crypto_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/synchronization/notification.h"
#include "src/core/interface/errors.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"
#include "src/public/cpio/adapters/crypto_client/mock_crypto_client_with_overrides.h"
#include "src/public/cpio/interface/crypto_client/crypto_client_interface.h"
#include "src/public/cpio/interface/crypto_client/type_def.h"
#include "src/public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

using google::cmrt::sdk::crypto_service::v1::AeadDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::cpio::CryptoClient;
using google::scp::cpio::CryptoClientOptions;
using google::scp::cpio::client_providers::mock::MockCryptoClientProvider;
using google::scp::cpio::mock::MockCryptoClientWithOverrides;
using testing::Return;

namespace google::scp::cpio::test {
class CryptoClientTest : public ::testing::Test {
 protected:
  CryptoClientTest() : client_(std::make_shared<CryptoClientOptions>()) {
    EXPECT_CALL(*client_.GetCryptoClientProvider(), Init)
        .WillOnce(Return(SuccessExecutionResult()));
    EXPECT_CALL(*client_.GetCryptoClientProvider(), Run)
        .WillOnce(Return(SuccessExecutionResult()));
    EXPECT_CALL(*client_.GetCryptoClientProvider(), Stop)
        .WillOnce(Return(SuccessExecutionResult()));

    EXPECT_THAT(client_.Init(), IsSuccessful());
    EXPECT_THAT(client_.Run(), IsSuccessful());
  }

  ~CryptoClientTest() { EXPECT_THAT(client_.Stop(), IsSuccessful()); }

  MockCryptoClientWithOverrides client_;
};

TEST_F(CryptoClientTest, HpkeEncryptSuccess) {
  EXPECT_CALL(*client_.GetCryptoClientProvider(), HpkeEncrypt)
      .WillOnce(
          [=](AsyncContext<HpkeEncryptRequest, HpkeEncryptResponse>& context) {
            context.response = std::make_shared<HpkeEncryptResponse>();
            context.Finish(SuccessExecutionResult());
            return SuccessExecutionResult();
          });

  absl::Notification finished;
  EXPECT_THAT(client_.HpkeEncrypt(HpkeEncryptRequest(),
                                  [&](const ExecutionResult result,
                                      HpkeEncryptResponse response) {
                                    EXPECT_THAT(result, IsSuccessful());
                                    finished.Notify();
                                  }),
              IsSuccessful());
  finished.WaitForNotification();
}

TEST_F(CryptoClientTest, HpkeEncryptFailure) {
  EXPECT_CALL(*client_.GetCryptoClientProvider(), HpkeEncrypt)
      .WillOnce(
          [=](AsyncContext<HpkeEncryptRequest, HpkeEncryptResponse>& context) {
            context.Finish(FailureExecutionResult(SC_UNKNOWN));
            return FailureExecutionResult(SC_UNKNOWN);
          });

  absl::Notification finished;
  EXPECT_THAT(
      client_.HpkeEncrypt(
          HpkeEncryptRequest(),
          [&](const ExecutionResult result, HpkeEncryptResponse response) {
            EXPECT_THAT(result, ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            finished.Notify();
          }),
      ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  finished.WaitForNotification();
}

TEST_F(CryptoClientTest, HpkeDecryptSuccess) {
  EXPECT_CALL(*client_.GetCryptoClientProvider(), HpkeDecrypt)
      .WillOnce(
          [=](AsyncContext<HpkeDecryptRequest, HpkeDecryptResponse>& context) {
            context.response = std::make_shared<HpkeDecryptResponse>();
            context.Finish(SuccessExecutionResult());
            return SuccessExecutionResult();
          });

  absl::Notification finished;
  EXPECT_THAT(client_.HpkeDecrypt(HpkeDecryptRequest(),
                                  [&](const ExecutionResult result,
                                      HpkeDecryptResponse response) {
                                    EXPECT_THAT(result, IsSuccessful());
                                    finished.Notify();
                                  }),
              IsSuccessful());
  finished.WaitForNotification();
}

TEST_F(CryptoClientTest, HpkeDecryptFailure) {
  EXPECT_CALL(*client_.GetCryptoClientProvider(), HpkeDecrypt)
      .WillOnce(
          [=](AsyncContext<HpkeDecryptRequest, HpkeDecryptResponse>& context) {
            context.Finish(FailureExecutionResult(SC_UNKNOWN));
            return FailureExecutionResult(SC_UNKNOWN);
          });

  absl::Notification finished;
  EXPECT_THAT(
      client_.HpkeDecrypt(
          HpkeDecryptRequest(),
          [&](const ExecutionResult result, HpkeDecryptResponse response) {
            EXPECT_THAT(result, ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            finished.Notify();
          }),
      ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  finished.WaitForNotification();
}

TEST_F(CryptoClientTest, AeadEncryptSuccess) {
  EXPECT_CALL(*client_.GetCryptoClientProvider(), AeadEncrypt)
      .WillOnce(
          [=](AsyncContext<AeadEncryptRequest, AeadEncryptResponse>& context) {
            context.response = std::make_shared<AeadEncryptResponse>();
            context.Finish(SuccessExecutionResult());
            return SuccessExecutionResult();
          });

  absl::Notification finished;
  EXPECT_THAT(client_.AeadEncrypt(AeadEncryptRequest(),
                                  [&](const ExecutionResult result,
                                      AeadEncryptResponse response) {
                                    EXPECT_THAT(result, IsSuccessful());
                                    finished.Notify();
                                  }),
              IsSuccessful());
  finished.WaitForNotification();
}

TEST_F(CryptoClientTest, AeadEncryptFailure) {
  EXPECT_CALL(*client_.GetCryptoClientProvider(), AeadEncrypt)
      .WillOnce(
          [=](AsyncContext<AeadEncryptRequest, AeadEncryptResponse>& context) {
            context.Finish(FailureExecutionResult(SC_UNKNOWN));
            return FailureExecutionResult(SC_UNKNOWN);
          });

  absl::Notification finished;
  EXPECT_THAT(
      client_.AeadEncrypt(
          AeadEncryptRequest(),
          [&](const ExecutionResult result, AeadEncryptResponse response) {
            EXPECT_THAT(result, ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            finished.Notify();
          }),
      ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  finished.WaitForNotification();
}

TEST_F(CryptoClientTest, AeadDecryptSuccess) {
  EXPECT_CALL(*client_.GetCryptoClientProvider(), AeadDecrypt)
      .WillOnce(
          [=](AsyncContext<AeadDecryptRequest, AeadDecryptResponse>& context) {
            context.response = std::make_shared<AeadDecryptResponse>();
            context.Finish(SuccessExecutionResult());
            return SuccessExecutionResult();
          });

  absl::Notification finished;
  EXPECT_THAT(client_.AeadDecrypt(AeadDecryptRequest(),
                                  [&](const ExecutionResult result,
                                      AeadDecryptResponse response) {
                                    EXPECT_THAT(result, IsSuccessful());
                                    finished.Notify();
                                  }),
              IsSuccessful());
  finished.WaitForNotification();
}

TEST_F(CryptoClientTest, AeadDecryptFailure) {
  EXPECT_CALL(*client_.GetCryptoClientProvider(), AeadDecrypt)
      .WillOnce(
          [=](AsyncContext<AeadDecryptRequest, AeadDecryptResponse>& context) {
            context.Finish(FailureExecutionResult(SC_UNKNOWN));
            return FailureExecutionResult(SC_UNKNOWN);
          });

  absl::Notification finished;
  EXPECT_THAT(
      client_.AeadDecrypt(
          AeadDecryptRequest(),
          [&](const ExecutionResult result, AeadDecryptResponse response) {
            EXPECT_THAT(result, ResultIs(FailureExecutionResult(SC_UNKNOWN)));
            finished.Notify();
          }),
      ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  finished.WaitForNotification();
}
}  // namespace google::scp::cpio::test
