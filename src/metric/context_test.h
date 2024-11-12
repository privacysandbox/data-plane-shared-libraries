//  Copyright 2022 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/log/check.h"
#include "src/metric/context.h"

namespace privacy_sandbox::server_common::metrics {

using ::testing::_;
using ::testing::A;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Exactly;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Pair;
using ::testing::Ref;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SizeIs;
using ::testing::StartsWith;
using ::testing::StrictMock;

using DefinitionSafe =
    Definition<int, Privacy::kNonImpacting, Instrument::kUpDownCounter>;
using DefinitionUnSafe =
    Definition<int, Privacy::kImpacting, Instrument::kUpDownCounter>;
using DefinitionPartition =
    Definition<int, Privacy::kNonImpacting, Instrument::kPartitionedCounter>;
using DefinitionPartitionUnsafe =
    Definition<int, Privacy::kImpacting, Instrument::kPartitionedCounter>;
using DefinitionHistogram =
    Definition<int, Privacy::kNonImpacting, Instrument::kHistogram>;
using DefinitionGauge =
    Definition<int, Privacy::kNonImpacting, Instrument::kGauge>;
using DefinitionCustom =
    Definition<double, Privacy::kImpacting, Instrument::kPartitionedCounter>;

inline constexpr DefinitionSafe kIntExactCounter("kIntExactCounter", "");
inline constexpr DefinitionSafe kIntExactCounter2("kIntExactCounter2", "");
inline constexpr DefinitionUnSafe kIntApproximateCounter(
    "kIntApproximateCounter", "", 0, 1);
inline constexpr DefinitionUnSafe kIntApproximateCounter2(
    "kIntApproximateCounter2", "", 0, 1);

inline constexpr std::string_view pv[] = {"buyer_1", "buyer_2", "buyer_3",
                                          "buyer_4", "buyer_5", "buyer_6"};
inline constexpr DefinitionPartition kIntExactPartitioned(
    "kIntExactPartitioned", "", "buyer_name", pv);
inline constexpr DefinitionPartition kIntExactAnyPartitioned(
    "kIntExactPartitioned", "", "buyer_name", kEmptyPublicPartition);
inline constexpr DefinitionPartitionUnsafe kIntUnSafePartitioned(
    "kIntUnSafePartitioned", "", "buyer_name", 5, pv, 1, 1);
inline constexpr DefinitionPartitionUnsafe kIntUnSafePrivatePartitioned(
    "kIntUnSafePrivatePartitioned", "", "buyer_name", 5, kEmptyPublicPartition,
    1, 1);
inline constexpr double hb[] = {50, 100, 200};
inline constexpr DefinitionHistogram kIntExactHistogram("kIntExactHistogram",
                                                        "", hb);

inline constexpr DefinitionGauge kIntExactGauge("kIntExactGauge", "");

inline constexpr const DefinitionName* metric_list[] = {
    &kIntExactCounter,
    &kIntExactCounter2,
    &kIntApproximateCounter,
    &kIntApproximateCounter2,
    &kIntExactPartitioned,
    &kIntUnSafePartitioned,
    &kIntExactHistogram,
    &kIntExactGauge,
    &kIntExactAnyPartitioned,
    &kCustom1,
    &kCustom2,
    &kCustom3};
inline constexpr absl::Span<const DefinitionName* const> metric_list_span =
    metric_list;
[[maybe_unused]] inline constexpr DefinitionSafe kNotInList("kNotInList", "");

class MockMetricRouter {
 public:
  MOCK_METHOD(absl::Status, LogSafe,
              (/*definition=*/(const DefinitionSafe&), /*value=*/int,
               /*partition=*/std::string_view,
               /*attributes=*/(absl::flat_hash_map<std::string, std::string>)));
  MOCK_METHOD(absl::Status, LogSafe,
              ((const DefinitionUnSafe&), int, std::string_view,
               (absl::flat_hash_map<std::string, std::string>)));
  MOCK_METHOD(absl::Status, LogSafe,
              ((const DefinitionPartition&), int, std::string_view,
               (absl::flat_hash_map<std::string, std::string>)));
  MOCK_METHOD(absl::Status, LogSafe,
              ((const DefinitionPartitionUnsafe&), int, std::string_view,
               (absl::flat_hash_map<std::string, std::string>)));
  MOCK_METHOD(absl::Status, LogSafe,
              ((const DefinitionHistogram&), int, std::string_view,
               (absl::flat_hash_map<std::string, std::string>)));
  MOCK_METHOD(absl::Status, LogSafe,
              ((const DefinitionGauge&), int, std::string_view,
               (absl::flat_hash_map<std::string, std::string>)));
  MOCK_METHOD(absl::Status, LogUnSafe,
              ((const DefinitionUnSafe&), int, std::string_view));
  MOCK_METHOD(absl::Status, LogUnSafe,
              ((const DefinitionSafe&), int, std::string_view));
  MOCK_METHOD(absl::Status, LogUnSafe,
              ((const DefinitionPartition&), int, std::string_view));
  MOCK_METHOD(absl::Status, LogUnSafe,
              ((const DefinitionPartitionUnsafe&), int, std::string_view));
  MOCK_METHOD(absl::Status, LogUnSafe,
              ((const DefinitionHistogram&), int, std::string_view));
  MOCK_METHOD(absl::Status, LogUnSafe,
              ((const DefinitionGauge&), int, std::string_view));
  MOCK_METHOD(telemetry::BuildDependentConfig&, metric_config, ());
  MOCK_METHOD(absl::Status, LogUnSafe,
              ((const DefinitionCustom&), double, std::string_view));
  MOCK_METHOD(absl::Status, LogSafe,
              ((const DefinitionCustom&), double, std::string_view,
               (absl::flat_hash_map<std::string, std::string>)));
};

class BaseTest : public ::testing::Test {
 protected:
  void InitConfig(telemetry::TelemetryConfig::TelemetryMode mode) {
    telemetry::TelemetryConfig config_proto;
    config_proto.set_mode(mode);
    auto* proto1 = config_proto.add_custom_udf_metric();
    auto* proto2 = config_proto.add_custom_udf_metric();
    auto* proto3 = config_proto.add_custom_udf_metric();

    proto1->set_name("udf_1");
    proto1->set_description("log_1");
    proto1->set_upper_bound(1);
    proto1->set_lower_bound(0);
    proto1->add_public_partitions("p_1");

    proto2->set_name("udf_2");
    proto2->set_description("log_2");
    proto2->set_upper_bound(2);
    proto2->set_lower_bound(0);
    proto2->add_public_partitions("p_2");
    proto2->add_public_partitions("p_3");

    proto3->set_name("udf_3");
    proto3->set_description("log_3");
    proto3->set_upper_bound(1);
    proto3->set_lower_bound(0);

    metric_config_ =
        std::make_unique<telemetry::BuildDependentConfig>(config_proto);
    EXPECT_CALL(mock_metric_router_, metric_config())
        .WillRepeatedly(ReturnRef(*metric_config_));
  }

  void SetUp() override {
    InitConfig(telemetry::TelemetryConfig::PROD);
    context_ = Context<metric_list_span, MockMetricRouter>::GetContext(
        &mock_metric_router_);
  }

  template <typename ValueT, typename MetricT>
  std::vector<std::pair<std::string, ValueT>> BoundPartitionsContributed(
      const absl::flat_hash_map<std::string, ValueT>& value,
      const MetricT& definition) {
    return context_->BoundPartitionsContributed(value, definition);
  }

  std::unique_ptr<telemetry::BuildDependentConfig> metric_config_;
  StrictMock<MockMetricRouter> mock_metric_router_;
  std::unique_ptr<Context<metric_list_span, MockMetricRouter>> context_;
};

class SafeMetricOnlyTest : public BaseTest {
 protected:
  void SetUp() override {
    InitConfig(telemetry::TelemetryConfig::PROD);
    safe_only_context_ =
        Context<metric_list_span, MockMetricRouter,
                /*safe_metric_only=*/true>::GetContext(&mock_metric_router_);
  }

  std::unique_ptr<
      Context<metric_list_span, MockMetricRouter, /*safe_metric_only=*/true>>
      safe_only_context_;
};

class ContextTest : public BaseTest {
 protected:
  void LogUnSafeForApproximate() {
    EXPECT_CALL(
        mock_metric_router_,
        LogUnSafe(Matcher<const DefinitionUnSafe&>(Ref(kIntApproximateCounter)),
                  Eq(1), _))
        .WillOnce(Return(absl::OkStatus()));
    CHECK_OK(context_->LogMetric<kIntApproximateCounter>(1));

    CHECK_OK(context_->LogMetricDeferred<kIntApproximateCounter2>(
        []() mutable { return 2; }));
    EXPECT_CALL(mock_metric_router_,
                LogUnSafe(Matcher<const DefinitionUnSafe&>(
                              Ref(kIntApproximateCounter2)),
                          Eq(2), _))
        .WillOnce(Return(absl::OkStatus()));
  }

  void LogSafeOK() {
    EXPECT_CALL(mock_metric_router_,
                LogSafe(Matcher<const DefinitionSafe&>(Ref(kIntExactCounter)),
                        Eq(1), _, _))
        .WillOnce(Return(absl::OkStatus()));
    CHECK_OK(context_->LogMetric<kIntExactCounter>(1));
    CHECK_OK(context_->LogMetricDeferred<kIntExactCounter2>(
        []() mutable { return 2; }));
    EXPECT_CALL(mock_metric_router_,
                LogSafe(Matcher<const DefinitionSafe&>(Ref(kIntExactCounter2)),
                        Eq(2), _, _))
        .WillOnce(Return(absl::OkStatus()));
  }

  void ErrorLogSafeAfterDecrypt() {
    EXPECT_EQ(context_->LogMetric<kIntExactCounter>(1).code(),
              absl::StatusCode::kFailedPrecondition);
    EXPECT_EQ(
        context_
            ->LogMetricDeferred<kIntExactCounter2>([]() mutable { return 2; })
            .code(),
        absl::StatusCode::kFailedPrecondition);
  }
};

class MetricConfigTest : public ::testing::Test {
 protected:
  void SetUpWithConfig(telemetry::BuildDependentConfig& metric_config) {
    EXPECT_CALL(mock_metric_router_, metric_config())
        .WillRepeatedly(ReturnRef(metric_config));
    context_ = Context<metric_list_span, MockMetricRouter>::GetContext(
        &mock_metric_router_);
  }

  StrictMock<MockMetricRouter> mock_metric_router_;
  std::unique_ptr<Context<metric_list_span, MockMetricRouter>> context_;
};

}  // namespace privacy_sandbox::server_common::metrics
