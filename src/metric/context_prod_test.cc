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

#include "src/metric/context_test.h"

namespace privacy_sandbox::server_common::metrics {

TEST_F(BaseTest, RequestStatus) {
  absl::Status err = absl::UnknownError("xyz");
  context_->SetRequestResult(err);
  EXPECT_EQ(context_->request_result(), err);
}

TEST_F(BaseTest, RequestCustomState) {
  context_->SetCustomState("xyz_key", "foo_val");
  auto ret = context_->CustomState("xyz_key");
  CHECK_OK(ret);
  EXPECT_EQ(*ret, "foo_val");
  EXPECT_EQ(context_->CustomState("not exist").status().code(),
            absl::StatusCode::kNotFound);
}

TEST_F(BaseTest, BoundPartitionsContributed) {
  absl::flat_hash_map<std::string, int> m;
  for (int i = 1; i < 10; ++i) {
    m.emplace(absl::StrCat("buyer_", i), i);
    std::vector<std::pair<std::string, int>> ret1 =
        BoundPartitionsContributed(m, kIntUnSafePartitioned);
    EXPECT_EQ(ret1.size(),
              std::min(kIntUnSafePartitioned.max_partitions_contributed_, i));
  }
}

TEST_F(BaseTest, FilterNotDefinedPartition) {
  absl::flat_hash_map<std::string, int> m = {{"private partition", 1}};
  EXPECT_THAT(BoundPartitionsContributed(m, kIntUnSafePartitioned), IsEmpty());
  EXPECT_THAT(BoundPartitionsContributed(m, kIntUnSafePrivatePartitioned),
              SizeIs(1));
}

TEST_F(BaseTest, LogUDFMetrics) {
  EXPECT_CALL(
      mock_metric_router_,
      LogUnSafe(Matcher<const DefinitionCustom&>(Ref(kCustom1)), Eq(1), "p_1"))
      .WillOnce(Return(absl::OkStatus()));

  EXPECT_CALL(
      mock_metric_router_,
      LogUnSafe(Matcher<const DefinitionCustom&>(Ref(kCustom2)), Eq(7), "p_2"))
      .WillOnce(Return(absl::OkStatus()));

  EXPECT_CALL(
      mock_metric_router_,
      LogUnSafe(Matcher<const DefinitionCustom&>(Ref(kCustom3)), Eq(1), ""))
      .WillOnce(Return(absl::OkStatus()));

  EXPECT_CALL(
      mock_metric_router_,
      LogUnSafe(Matcher<const DefinitionCustom&>(Ref(kCustom2)), Eq(0), "p_3"))
      .Times(0);

  BatchUDFMetric udf_metrics1, udf_metrics2, udf_metrics3;
  auto udf_metric1 = udf_metrics1.add_udf_metric();
  auto udf_metric2 = udf_metrics1.add_udf_metric();
  auto udf_metric3 = udf_metrics2.add_udf_metric();
  auto udf_metric2a = udf_metrics2.add_udf_metric();
  auto udf_metric4 = udf_metrics2.add_udf_metric();
  auto udf_metric5 = udf_metrics3.add_udf_metric();

  udf_metric1->set_name("udf_1");
  udf_metric1->set_value(1);
  udf_metric1->set_public_partition("p_1");

  udf_metric2->set_name("udf_2");
  udf_metric2->set_value(4);
  udf_metric2->set_public_partition("p_2");

  udf_metric2a->set_name("udf_2");
  udf_metric2a->set_value(3);
  udf_metric2a->set_public_partition("p_2");

  udf_metric3->set_name("undefined_1");
  udf_metric4->set_name("undefined_2");

  udf_metric5->set_name("udf_3");
  udf_metric5->set_value(1);

  CHECK_OK(context_->LogUDFMetrics(udf_metrics1));

  CHECK_OK(context_->LogUDFMetrics(udf_metrics3));
  EXPECT_THAT(context_->LogUDFMetrics(udf_metrics2).message(),
              "name not found: undefined_1,undefined_2");
}

TEST_F(BaseTest, LogUpDownCounter) {
  EXPECT_CALL(mock_metric_router_,
              LogSafe(Matcher<const DefinitionSafe&>(Ref(kIntExactCounter)),
                      Eq(1), _, _))
      .WillOnce(Return(absl::OkStatus()));
  CHECK_OK(context_->LogUpDownCounter<kIntExactCounter>(1));
  // auto e1 = context_->LogUpDownCounter<kIntExactGauge>(1);  // compile errors
  // auto e2 = context_->LogUpDownCounterDeferred<kIntExactGauge>(
  //     []() mutable { return 2; });  // compile errors
}

TEST_F(BaseTest, LogUpDownCounterDeferred) {
  CHECK_OK(context_->LogUpDownCounterDeferred<kIntExactCounter>(
      []() mutable { return 2; }));
  EXPECT_CALL(mock_metric_router_,
              LogSafe(Matcher<const DefinitionSafe&>(Ref(kIntExactCounter)),
                      Eq(2), _, _))
      .WillOnce(Return(absl::OkStatus()));
}

TEST_F(BaseTest, LogPartitionedCounter) {
  EXPECT_CALL(
      mock_metric_router_,
      LogSafe(Matcher<const DefinitionPartition&>(Ref(kIntExactPartitioned)),
              Eq(1), StartsWith("buyer_"), _))
      .Times(Exactly(2))
      .WillRepeatedly(Return(absl::OkStatus()));
  CHECK_OK(context_->LogUpDownCounter<kIntExactPartitioned>(
      {{"buyer_1", 1}, {"buyer_2", 1}}));
  // auto e1 = context_->LogUpDownCounter<kIntExactCounter>(
  //     {{"buyer_1", 1}, {"buyer_2", 1}});  // compile errors
  // auto e2 = context_->LogUpDownCounterDeferred<kIntExactCounter>(
  //     []() -> absl::flat_hash_map<std::string, int> {
  //       return {{"buyer_3", 2}, {"buyer_4", 2}};
  //     });  // compile errors
}

TEST_F(BaseTest, LogPartitionedCounterDeferred) {
  CHECK_OK(context_->LogUpDownCounterDeferred<kIntExactPartitioned>(
      []() -> absl::flat_hash_map<std::string, int> {
        return {{"buyer_3", 2}, {"buyer_4", 2}};
      }));
  EXPECT_CALL(
      mock_metric_router_,
      LogSafe(Matcher<const DefinitionPartition&>(Ref(kIntExactPartitioned)),
              Eq(2), StartsWith("buyer_"), _))
      .Times(Exactly(2))
      .WillRepeatedly(Return(absl::OkStatus()));
}

TEST_F(BaseTest, LogHistogram) {
  EXPECT_CALL(
      mock_metric_router_,
      LogSafe(Matcher<const DefinitionHistogram&>(Ref(kIntExactHistogram)),
              Eq(1), _, _))
      .WillOnce(Return(absl::OkStatus()));
  CHECK_OK(context_->LogHistogram<kIntExactHistogram>(1));
  // auto e1 = context_->LogHistogram<kIntExactGauge>(1);  // compile errors
  // auto e2 = context_->LogHistogramDeferred<kIntExactGauge>(
  //     []() mutable { return 2; });  // compile errors
}

TEST_F(BaseTest, LogHistogramDeferred) {
  CHECK_OK(context_->LogHistogramDeferred<kIntExactHistogram>(
      []() mutable { return 2; }));
  EXPECT_CALL(
      mock_metric_router_,
      LogSafe(Matcher<const DefinitionHistogram&>(Ref(kIntExactHistogram)),
              Eq(2), _, _))
      .WillOnce(Return(absl::OkStatus()));
}

TEST_F(BaseTest, LogGauge) {
  EXPECT_CALL(mock_metric_router_,
              LogSafe(Matcher<const DefinitionGauge&>(Ref(kIntExactGauge)),
                      Eq(1), _, _))
      .WillOnce(Return(absl::OkStatus()));
  CHECK_OK(context_->LogGauge<kIntExactGauge>(1));
  // auto e1 = context_->LogGauge<kIntExactCounter>(1);  // compile errors
  // auto e2 = context_->LogGaugeDeferred<kIntExactCounter>(
  //     []() mutable { return 2; });  // compile errors
}

TEST_F(BaseTest, LogGaugeDeferred) {
  CHECK_OK(
      context_->LogGaugeDeferred<kIntExactGauge>([]() mutable { return 2; }));
  EXPECT_CALL(mock_metric_router_,
              LogSafe(Matcher<const DefinitionGauge&>(Ref(kIntExactGauge)),
                      Eq(2), _, _))
      .WillOnce(Return(absl::OkStatus()));
}

TEST_F(BaseTest, LogSafePartitionedNotBounded) {
  absl::flat_hash_map<std::string, int> m;
  constexpr int kTotal = 100;
  for (int i = 0; i < kTotal; ++i) {
    m.emplace(absl::StrCat("buyer_xyz_", i), i);
  }
  EXPECT_CALL(
      mock_metric_router_,
      LogSafe(Matcher<const DefinitionPartition&>(Ref(kIntExactAnyPartitioned)),
              A<int>(), StartsWith("buyer_"), _))
      .Times(Exactly(kTotal))
      .WillRepeatedly(Return(absl::OkStatus()));
  CHECK_OK(context_->LogUpDownCounter<kIntExactAnyPartitioned>(m));
}

TEST_F(BaseTest, LogPartitionedFiltered) {
  EXPECT_CALL(mock_metric_router_,
              LogUnSafe(Matcher<const DefinitionPartitionUnsafe&>(
                            Ref(kIntUnSafePartitioned)),
                        Eq(1), StartsWith("buyer_1")))
      .WillOnce(Return(absl::OkStatus()));
  CHECK_OK(context_->LogUpDownCounter<kIntUnSafePartitioned>(
      {{"buyer_1", 1}, {"buyer_100", 1}}));
}

TEST_F(BaseTest, LogPartitionedBounded) {
  absl::flat_hash_map<std::string, int> m;
  for (int i = 1; i < 10; ++i) {
    m.emplace(absl::StrCat("buyer_", i), i);
  }
  EXPECT_CALL(mock_metric_router_,
              LogUnSafe(Matcher<const DefinitionPartitionUnsafe&>(
                            Ref(kIntUnSafePartitioned)),
                        A<int>(), StartsWith("buyer_")))
      .Times(Exactly(kIntUnSafePartitioned.max_partitions_contributed_))
      .WillRepeatedly(Return(absl::OkStatus()));
  CHECK_OK(context_->LogUpDownCounter<kIntUnSafePartitioned>(m));
}

TEST_F(BaseTest, LogPartitionedCounterDeferredBounded) {
  CHECK_OK(context_->LogUpDownCounterDeferred<kIntUnSafePartitioned>([]() {
    absl::flat_hash_map<std::string, int> m;
    for (int i = 1; i < 10; ++i) {
      m.emplace(absl::StrCat("buyer_", i), i);
    }
    return m;
  }));
  EXPECT_CALL(mock_metric_router_,
              LogUnSafe(Matcher<const DefinitionPartitionUnsafe&>(
                            Ref(kIntUnSafePartitioned)),
                        A<int>(), StartsWith("buyer_")))
      .Times(Exactly(kIntUnSafePartitioned.max_partitions_contributed_))
      .WillRepeatedly(Return(absl::OkStatus()));
}

TEST_F(ContextTest, LogBeforeDecrypt) {
  EXPECT_CALL(mock_metric_router_,
              LogSafe(Matcher<const DefinitionSafe&>(Ref(kIntExactCounter)),
                      Eq(1), _, _))
      .WillOnce(Return(absl::OkStatus()));
  CHECK_OK(context_->LogMetric<kIntExactCounter>(1));
  // auto e1 = context_->LogMetric<kIntExactCounter>(1.2);  // compile errors
  // auto e2 = context_->LogMetric<kNotInList>(1);          // compile errors

  CHECK_OK(context_->LogMetricDeferred<kIntExactCounter2>(
      []() mutable { return 2; }));
  EXPECT_CALL(mock_metric_router_,
              LogSafe(Matcher<const DefinitionSafe&>(Ref(kIntExactCounter2)),
                      Eq(2), _, _))
      .WillOnce(Return(absl::OkStatus()));
  // absl::AnyInvocable<int() &&> cb = []() mutable { return 2; };
  // auto e1 = context_->LogMetricDeferred<kIntExactCounter>(cb);  // compile
  // error

  LogUnSafeForApproximate();
}

TEST_F(ContextTest, LogAfterDecrypt) {
  context_->SetDecrypted();
  ErrorLogSafeAfterDecrypt();
  LogUnSafeForApproximate();
}

TEST_F(BaseTest, Accumulate) {
  EXPECT_CALL(
      mock_metric_router_,
      LogUnSafe(Matcher<const DefinitionUnSafe&>(Ref(kIntApproximateCounter)),
                Eq(101), _))
      .WillOnce(Return(absl::OkStatus()));
  CHECK_OK(context_->AccumulateMetric<kIntApproximateCounter>(1));
  CHECK_OK(context_->AccumulateMetric<kIntApproximateCounter>(100));
  // compile errors:
  // CHECK_OK(context_->AccumulateMetric<kIntApproximateCounter>(1.2));
  // CHECK_OK(context_->AccumulateMetric<kNotInList>(1))  ;
  // CHECK_OK(context_->AccumulateMetric<kIntExactCounter>(1));
}

TEST_F(BaseTest, AggregateToGetMeanCounterInstrument) {
  EXPECT_CALL(
      mock_metric_router_,
      LogUnSafe(Matcher<const DefinitionUnSafe&>(Ref(kIntApproximateCounter)),
                Eq(4), _))
      .WillOnce(Return(absl::OkStatus()));
  CHECK_OK(context_->AggregateMetricToGetMean<kIntApproximateCounter>(5));
  CHECK_OK(context_->AggregateMetricToGetMean<kIntApproximateCounter>(3));
}

TEST_F(BaseTest, AggregateToGetMeanHistogramInstrument) {
  EXPECT_CALL(
      mock_metric_router_,
      LogSafe(Matcher<const DefinitionHistogram&>(Ref(kIntExactHistogram)),
              Eq(4), _, _))
      .WillOnce(Return(absl::OkStatus()));
  CHECK_OK(context_->AggregateMetricToGetMean<kIntExactHistogram>(5));
  CHECK_OK(context_->AggregateMetricToGetMean<kIntExactHistogram>(3));
}

TEST_F(BaseTest, AggregateToGetMeanForDifferentPartitions) {
  EXPECT_CALL(mock_metric_router_,
              LogUnSafe(Matcher<const DefinitionPartitionUnsafe&>(
                            Ref(kIntUnSafePartitioned)),
                        Eq(3), "buyer_1"))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_metric_router_,
              LogUnSafe(Matcher<const DefinitionPartitionUnsafe&>(
                            Ref(kIntUnSafePartitioned)),
                        Eq(4), "buyer_2"))
      .WillOnce(Return(absl::OkStatus()));
  CHECK_OK(
      context_->AggregateMetricToGetMean<kIntUnSafePartitioned>(2, "buyer_1"));
  CHECK_OK(
      context_->AggregateMetricToGetMean<kIntUnSafePartitioned>(4, "buyer_1"));
  CHECK_OK(
      context_->AggregateMetricToGetMean<kIntUnSafePartitioned>(4, "buyer_2"));
}

TEST_F(BaseTest, AccumulatePartition) {
  EXPECT_CALL(mock_metric_router_,
              LogUnSafe(Matcher<const DefinitionPartitionUnsafe&>(
                            Ref(kIntUnSafePartitioned)),
                        Eq(101), "buyer_1"))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(mock_metric_router_,
              LogUnSafe(Matcher<const DefinitionPartitionUnsafe&>(
                            Ref(kIntUnSafePartitioned)),
                        Eq(200), "buyer_2"))
      .WillOnce(Return(absl::OkStatus()));
  CHECK_OK(context_->AccumulateMetric<kIntUnSafePartitioned>(1, "buyer_1"));
  CHECK_OK(context_->AccumulateMetric<kIntUnSafePartitioned>(100, "buyer_1"));
  CHECK_OK(context_->AccumulateMetric<kIntUnSafePartitioned>(200, "buyer_2"));
}

TEST_F(MetricConfigTest, ConfigMetricList) {
  telemetry::TelemetryConfig config_proto;
  config_proto.set_mode(telemetry::TelemetryConfig::PROD);
  config_proto.add_metric()->set_name("kIntExactCounter");
  telemetry::BuildDependentConfig metric_config(config_proto);
  SetUpWithConfig(metric_config);

  EXPECT_CALL(mock_metric_router_,
              LogSafe(Matcher<const DefinitionSafe&>(Ref(kIntExactCounter)),
                      Eq(1), _, _))
      .WillOnce(Return(absl::OkStatus()));
  CHECK_OK(context_->LogUpDownCounter<kIntExactCounter>(1));

  auto s = context_->LogUpDownCounter<kIntExactCounter2>(1);
  EXPECT_TRUE(absl::IsNotFound(s));
}

TEST_F(BaseTest, LogMultiTimesReturnError) {
  EXPECT_CALL(mock_metric_router_,
              LogSafe(Matcher<const DefinitionSafe&>(Ref(kIntExactCounter)),
                      Eq(1), _, _))
      .WillOnce(Return(absl::OkStatus()));
  CHECK_OK(context_->LogUpDownCounter<kIntExactCounter>(1));
  EXPECT_EQ(context_->LogUpDownCounter<kIntExactCounter>(1).code(),
            absl::StatusCode::kAlreadyExists);
}

TEST_F(BaseTest, LogMultiTimesDeferredReturnError) {
  CHECK_OK(context_->LogUpDownCounterDeferred<kIntExactCounter>(
      []() mutable { return 2; }));
  EXPECT_EQ(context_
                ->LogUpDownCounterDeferred<kIntExactCounter>(
                    []() mutable { return 2; })
                .code(),
            absl::StatusCode::kAlreadyExists);
  EXPECT_CALL(mock_metric_router_,
              LogSafe(Matcher<const DefinitionSafe&>(Ref(kIntExactCounter)),
                      Eq(2), _, _))
      .WillOnce(Return(absl::OkStatus()));
}

TEST_F(SafeMetricOnlyTest, LogMultiTimes) {
  constexpr int n = 10;
  EXPECT_CALL(mock_metric_router_,
              LogSafe(Matcher<const DefinitionSafe&>(Ref(kIntExactCounter)),
                      Eq(1), _, _))
      .Times(Exactly(n))
      .WillRepeatedly(Return(absl::OkStatus()));
  for (int i = 0; i < n; ++i) {
    CHECK_OK(safe_only_context_->LogUpDownCounter<kIntExactCounter>(1));
  }
  // compile error:
  // safe_only_context_->LogUpDownCounter<kIntApproximateCounter>(1);
}

TEST_F(SafeMetricOnlyTest, LogMultiTimesDeferred) {
  constexpr int n = 10;
  for (int i = 0; i < n; ++i) {
    CHECK_OK(safe_only_context_->LogUpDownCounterDeferred<kIntExactCounter>(
        []() mutable { return 2; }));
  }
  EXPECT_CALL(mock_metric_router_,
              LogSafe(Matcher<const DefinitionSafe&>(Ref(kIntExactCounter)),
                      Eq(2), _, _))
      .Times(Exactly(n))
      .WillRepeatedly(Return(absl::OkStatus()));
  // compile error:
  // safe_only_context_->LogUpDownCounterDeferred<kIntApproximateCounter>(
  //     []() mutable { return 2; });
}

}  // namespace privacy_sandbox::server_common::metrics
