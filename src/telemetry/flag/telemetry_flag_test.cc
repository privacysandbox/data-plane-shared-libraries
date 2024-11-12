// Copyright 2023 Google LLC
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

#include "src/telemetry/flag/telemetry_flag.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/log/absl_log.h"
#include "absl/log/check.h"
#include "google/protobuf/util/message_differencer.h"

namespace privacy_sandbox::server_common::telemetry {
namespace {

using google::protobuf::util::MessageDifferencer;

TEST(TelemetryFlag, Parse) {
  std::string_view flag_str = R"pb(mode: PROD
                                   metric { name: "m_0" }
                                   metric { name: "m_1" }
                                   metric_export_interval_ms: 100
                                   dp_export_interval_ms: 200)pb";
  TelemetryFlag f_parsed;
  std::string err;
  EXPECT_TRUE(AbslParseFlag(flag_str, &f_parsed, &err));
  EXPECT_EQ(f_parsed.server_config.mode(), TelemetryConfig::PROD);
  EXPECT_EQ(f_parsed.server_config.metric_size(), 2);
  EXPECT_EQ(f_parsed.server_config.metric_export_interval_ms(), 100);
  EXPECT_EQ(f_parsed.server_config.dp_export_interval_ms(), 200);
}

TEST(TelemetryFlag, ParseError) {
  TelemetryFlag f_parsed;
  std::string err;
  EXPECT_FALSE(AbslParseFlag("cause_error", &f_parsed, &err));
}

TEST(TelemetryFlag, ParseUnParse) {
  TelemetryFlag f;
  f.server_config.set_mode(TelemetryConfig::PROD);
  f.server_config.add_metric()->set_name("m_0");
  f.server_config.add_metric()->set_name("m_1");
  TelemetryFlag f_parsed;

  std::string err;
  AbslParseFlag(AbslUnparseFlag(f), &f_parsed, &err);
  EXPECT_TRUE(
      MessageDifferencer::Equals(f.server_config, f_parsed.server_config));
}

TEST(BuildDependentConfig, Off) {
  TelemetryConfig config_proto;
  config_proto.set_mode(TelemetryConfig::OFF);
  BuildDependentConfig config(config_proto);
  EXPECT_FALSE(config.MetricAllowed());
  EXPECT_FALSE(config.TraceAllowed());
  EXPECT_FALSE(config.LogsAllowed());
}

TEST(BuildDependentConfig, Prod) {
  TelemetryConfig config_proto;
  config_proto.set_mode(TelemetryConfig::PROD);
  BuildDependentConfig config(config_proto);
  EXPECT_TRUE(config.MetricAllowed());
  EXPECT_FALSE(config.TraceAllowed());
  EXPECT_EQ(config.metric_export_interval_ms(), 60000);
  EXPECT_GE(config.dp_export_interval_ms(), 300000);
  EXPECT_LE(config.dp_export_interval_ms(), 330000);
  EXPECT_TRUE(config.LogsAllowed());
}

TEST(BuildDependentConfig, EmptyMetricConfigAlwaysOK) {
  TelemetryConfig config_proto;
  BuildDependentConfig config(config_proto);
  CHECK_OK(config.GetMetricConfig("any_metric"));
}

TEST(BuildDependentConfig, MetricConfigFilterAllowed) {
  TelemetryConfig config_proto;
  config_proto.add_metric()->set_name("allowed");
  BuildDependentConfig config(config_proto);
  EXPECT_EQ(config.GetMetricConfig("any_metric").status().code(),
            absl::StatusCode::kNotFound);
  auto metric_config = config.GetMetricConfig("allowed");
  CHECK_OK(metric_config);
  EXPECT_EQ(metric_config->name(), "allowed");
}

constexpr metrics::Definition<int, metrics::Privacy::kNonImpacting,
                              metrics::Instrument::kUpDownCounter>
    c2("c2", "c21");
inline constexpr const metrics::DefinitionName* kList[] = {&c2};

TEST(BuildDependentConfig, CheckMetricConfigInList) {
  TelemetryConfig config_proto;
  config_proto.add_metric()->set_name("c2");
  BuildDependentConfig config(config_proto);
  CHECK_OK(config.CheckMetricConfig(kList));
  config_proto.add_metric()->set_name("c3");
  config_proto.add_metric()->set_name("c4");
  BuildDependentConfig config_not_defined(config_proto);
  EXPECT_EQ(config_not_defined.CheckMetricConfig(kList).code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(config_not_defined.CheckMetricConfig(kList).message(),
              testing::ContainsRegex("c3 not defined;"));
  EXPECT_THAT(config_not_defined.CheckMetricConfig(kList).message(),
              testing::ContainsRegex("c4 not defined;"));
}

constexpr std::string_view kDefaultBuyers[] = {"buyer_1", "buyer_2"};
constexpr metrics::Definition<int, metrics::Privacy::kNonImpacting,
                              metrics::Instrument::kPartitionedCounter>
    partition_metric("partition_metric", "", "partition_type", kDefaultBuyers);

TEST(BuildDependentConfig, Partition) {
  TelemetryConfig config_proto;
  BuildDependentConfig config(config_proto);

  EXPECT_THAT(config.GetPartition(partition_metric)->view(),
              testing::ElementsAreArray(kDefaultBuyers));

  constexpr std::string_view new_partitions[] = {"789", "456", "123"};
  config.SetPartition("another_metric_def", new_partitions);
  EXPECT_THAT(config.GetPartition(partition_metric)->view(),
              testing::ElementsAreArray(kDefaultBuyers));

  config.SetPartition(partition_metric.name_, new_partitions);
  // result is sorted of `new_partitions`
  EXPECT_THAT(config.GetPartition(partition_metric)->view(),
              testing::ElementsAreArray({"123", "456", "789"}));
}

TEST(BuildDependentConfig, SetMaxPartitionsContributed) {
  TelemetryConfig config_proto;
  BuildDependentConfig config(config_proto);
  config.SetMaxPartitionsContributed(partition_metric.name_, 5);
  EXPECT_EQ(config.GetMaxPartitionsContributed(partition_metric), 5);
}

constexpr metrics::Definition<int, metrics::Privacy::kImpacting,
                              metrics::Instrument::kPartitionedCounter>
    metric_1("m_1", "", "partition_type", 1, kDefaultBuyers, 1, 0, 0.95);

constexpr metrics::Definition<int, metrics::Privacy::kImpacting,
                              metrics::Instrument::kPartitionedCounter>
    metric_2("m_2", "", "partition_type", 2, kDefaultBuyers, 10, 2);

TEST(BuildDependentConfig, DefaultConfig) {
  TelemetryConfig config_proto;
  BuildDependentConfig config(config_proto);

  EXPECT_EQ(config_proto.metric_size(), 0);

  EXPECT_EQ(config.template GetDropNoisyValuesProbability(metric_1), 0.95);
  EXPECT_EQ(config.template GetMaxPartitionsContributed(metric_1), 1);
  EXPECT_EQ(config.template GetPrivacyBudgetWeight(metric_1), 1.0);
  EXPECT_EQ(config.template GetBound(metric_1).lower_bound_, 0);
  EXPECT_EQ(config.template GetBound(metric_1).upper_bound_, 1);

  EXPECT_EQ(config.template GetDropNoisyValuesProbability(metric_2), 0.0);
  EXPECT_EQ(config.template GetMaxPartitionsContributed(metric_2), 2);
  EXPECT_EQ(config.template GetPrivacyBudgetWeight(metric_2), 1.0);
  EXPECT_EQ(config.template GetBound(metric_2).lower_bound_, 2);
  EXPECT_EQ(config.template GetBound(metric_2).upper_bound_, 10);
}

TEST(BuildDependentConfig, CustomConfig) {
  TelemetryConfig config_proto;
  TelemetryConfig config_proto1;
  TelemetryConfig config_proto2;

  config_proto1.add_custom_udf_metric()->set_name("test_count_1");
  config_proto1.add_custom_udf_metric()->set_name("test_count_2");

  auto* proto1 = config_proto.add_custom_udf_metric();
  auto* proto2 = config_proto.add_custom_udf_metric();
  auto* proto3 = config_proto.add_custom_udf_metric();
  auto* proto4 = config_proto.add_custom_udf_metric();
  auto* proto5 = config_proto.add_custom_udf_metric();

  proto1->set_name("udf_1");
  proto1->set_description("log udf 1");
  proto1->set_upper_bound(100);
  proto1->set_lower_bound(0);
  proto1->set_privacy_budget_weight(1.0);
  proto1->add_public_partitions("buyer_1");
  proto1->add_public_partitions("buyer_2");
  proto1->set_partition_type("log_partition");
  proto1->set_max_partitions_contributed(2);

  proto2->set_name("udf_1");
  proto2->set_description("undefined");

  proto3->set_name("udf_3");
  proto3->set_description("log udf 3");
  proto3->set_privacy_budget_weight(0.5);

  proto4->set_name("udf_4");
  proto4->set_description("log udf 4");

  proto5->set_name("udf_5");
  proto5->set_description("log udf 5");
  proto5->set_privacy_budget_weight(0.5);

  BuildDependentConfig config1(config_proto1);
  EXPECT_EQ(config1.CustomMetricsWeight(), 2);
  EXPECT_EQ(config1.GetName(metrics::kCustom1), "test_count_1");
  EXPECT_EQ(config1.GetName(metrics::kCustom2), "test_count_2");
  EXPECT_EQ(config1.GetName(metrics::kCustom3), "placeholder_3");

  BuildDependentConfig config2(config_proto2);
  EXPECT_EQ(config2.CustomMetricsWeight(), 0);

  BuildDependentConfig config(config_proto);
  EXPECT_EQ(config.CustomMetricsWeight(), 2.5);

  EXPECT_EQ(config.GetName(metrics::kCustom1), "udf_1");
  EXPECT_EQ(config.GetDescription(metrics::kCustom1), "log udf 1");
  EXPECT_EQ(*config.GetCustomDefinition("udf_1"), &metrics::kCustom1);
  EXPECT_EQ(config.template GetPartitionType(metrics::kCustom1),
            "log_partition");
  EXPECT_EQ(config.template GetMaxPartitionsContributed(metrics::kCustom1), 2);
  EXPECT_EQ(config.template GetBound(metrics::kCustom1).lower_bound_, 0);
  EXPECT_EQ(config.template GetBound(metrics::kCustom1).upper_bound_, 100);
  EXPECT_EQ(config.template GetPrivacyBudgetWeight(metrics::kCustom1), 1.0);
  EXPECT_THAT(config.template GetPartition(metrics::kCustom1)->view(),
              testing::ElementsAreArray(kDefaultBuyers));

  EXPECT_EQ(config.GetName(metrics::kCustom2), "udf_3");
  EXPECT_EQ(config.GetDescription(metrics::kCustom2), "log udf 3");
  EXPECT_EQ(*config.GetCustomDefinition("udf_3"), &metrics::kCustom2);
  EXPECT_EQ(config.template GetPartitionType(metrics::kCustom2), "");
  EXPECT_EQ(config.template GetBound(metrics::kCustom2).lower_bound_, 0);
  EXPECT_EQ(config.template GetBound(metrics::kCustom2).upper_bound_, 1);
  EXPECT_EQ(config.template GetPrivacyBudgetWeight(metrics::kCustom2), 0.5);
  EXPECT_EQ(config.template GetMaxPartitionsContributed(metrics::kCustom2), 1);

  EXPECT_EQ(*config.GetCustomDefinition("udf_4"), &metrics::kCustom3);
  EXPECT_EQ(config.template GetPrivacyBudgetWeight(metrics::kCustom3), 1.0);
  EXPECT_EQ(config.GetCustomDefinition("udf_5").status().code(),
            absl::StatusCode::kNotFound);
}

TEST(BuildDependentConfig, HasFlagMaxPartitionsContributed) {
  TelemetryConfig config_proto;

  auto metric_config_1 = config_proto.add_metric();
  metric_config_1->set_name("m_1");
  metric_config_1->set_max_partitions_contributed(5);

  BuildDependentConfig config(config_proto);

  EXPECT_EQ(config_proto.metric_size(), 1);

  EXPECT_EQ(config.template GetDropNoisyValuesProbability(metric_1), 0.95);
  EXPECT_EQ(config.template GetMaxPartitionsContributed(metric_1), 5);
  EXPECT_EQ(config.template GetPrivacyBudgetWeight(metric_1), 1.0);
  EXPECT_EQ(config.template GetBound(metric_1).lower_bound_, 0);
  EXPECT_EQ(config.template GetBound(metric_1).upper_bound_, 1);
}

TEST(BuildDependentConfig, HasFlagMinNoiseToOutput) {
  TelemetryConfig config_proto;

  auto metric_config_1 = config_proto.add_metric();
  metric_config_1->set_name("m_1");
  metric_config_1->set_drop_noisy_values_probability(0.99);

  BuildDependentConfig config(config_proto);

  EXPECT_EQ(config_proto.metric_size(), 1);

  EXPECT_EQ(config.template GetDropNoisyValuesProbability(metric_1), 0.99);
  EXPECT_EQ(config.template GetMaxPartitionsContributed(metric_1), 1);
  EXPECT_EQ(config.template GetPrivacyBudgetWeight(metric_1), 1.0);
  EXPECT_EQ(config.template GetBound(metric_1).lower_bound_, 0);
  EXPECT_EQ(config.template GetBound(metric_1).upper_bound_, 1);
}

TEST(BuildDependentConfig, HasFlagPrivacyBudgetWeight) {
  TelemetryConfig config_proto;

  auto metric_config_1 = config_proto.add_metric();
  metric_config_1->set_name("m_1");
  metric_config_1->set_privacy_budget_weight(2.0);

  BuildDependentConfig config(config_proto);

  EXPECT_EQ(config_proto.metric_size(), 1);

  EXPECT_EQ(config.template GetDropNoisyValuesProbability(metric_1), 0.95);
  EXPECT_EQ(config.template GetMaxPartitionsContributed(metric_1), 1);
  EXPECT_EQ(config.template GetPrivacyBudgetWeight(metric_1), 2.0);
  EXPECT_EQ(config.template GetBound(metric_1).lower_bound_, 0);
  EXPECT_EQ(config.template GetBound(metric_1).upper_bound_, 1);
}

TEST(BuildDependentConfig, HasFlagNoiseBound) {
  constexpr metrics::Definition<int, metrics::Privacy::kImpacting,
                                metrics::Instrument::kPartitionedCounter>
      metric_3("m_3", "", "partition_type", 1, kDefaultBuyers, 3, 2);

  constexpr metrics::Definition<int, metrics::Privacy::kImpacting,
                                metrics::Instrument::kPartitionedCounter>
      metric_4("m_4", "", "partition_type", 1, kDefaultBuyers, 10, 2);

  constexpr metrics::Definition<int, metrics::Privacy::kImpacting,
                                metrics::Instrument::kPartitionedCounter>
      metric_5("m_5", "", "partition_type", 1, kDefaultBuyers, 10, 1);

  TelemetryConfig config_proto;

  auto metric_config_1 = config_proto.add_metric();
  metric_config_1->set_name("m_1");
  metric_config_1->set_upper_bound(3);

  auto metric_config_2 = config_proto.add_metric();
  metric_config_2->set_name("m_2");
  metric_config_2->set_lower_bound(1.9);
  metric_config_2->set_upper_bound(10.1);

  auto metric_config_3 = config_proto.add_metric();
  metric_config_3->set_name("m_3");
  metric_config_3->set_lower_bound(2.1);

  auto metric_config_4 = config_proto.add_metric();
  metric_config_4->set_name("m_4");
  metric_config_4->set_upper_bound(1.5);

  auto metric_config_5 = config_proto.add_metric();
  metric_config_5->set_name("m_5");
  metric_config_5->set_lower_bound(11);
  metric_config_5->set_upper_bound(10);

  BuildDependentConfig config(config_proto);

  EXPECT_EQ(config_proto.metric_size(), 5);

  EXPECT_EQ(config.template GetBound(metric_1).lower_bound_, 0);
  EXPECT_EQ(config.template GetBound(metric_1).upper_bound_, 3);

  EXPECT_EQ(config.template GetBound(metric_2).lower_bound_, 1.9);
  EXPECT_EQ(config.template GetBound(metric_2).upper_bound_, 10.1);

  EXPECT_EQ(config.template GetBound(metric_3).lower_bound_, 2.1);
  EXPECT_EQ(config.template GetBound(metric_3).upper_bound_, 3);

  EXPECT_EQ(config.template GetBound(metric_4).lower_bound_, 2);
  EXPECT_EQ(config.template GetBound(metric_4).upper_bound_, 10);

  EXPECT_EQ(config.template GetBound(metric_5).lower_bound_, 1);
  EXPECT_EQ(config.template GetBound(metric_5).upper_bound_, 10);
}

}  // namespace
}  // namespace privacy_sandbox::server_common::telemetry
