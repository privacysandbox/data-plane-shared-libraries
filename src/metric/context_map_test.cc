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

#include "src/metric/context_map.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/log/check.h"

namespace privacy_sandbox::server_common::metrics {
namespace {

using ::testing::HasSubstr;

constexpr Definition<int, Privacy::kNonImpacting, Instrument::kUpDownCounter>
    kIntExactCounter("kIntExactCounter", "description");
constexpr const DefinitionName* metric_list[] = {&kIntExactCounter};
constexpr absl::Span<const DefinitionName* const> metric_list_span =
    metric_list;

class TestMetricRouter {
 public:
  TestMetricRouter()
      : metric_config_(telemetry::BuildDependentConfig(config_proto_)) {}

  template <typename T, Privacy privacy, Instrument instrument>
  absl::Status LogSafe(T value,
                       const Definition<T, privacy, instrument>& definition,
                       std::string_view partition) {
    return absl::OkStatus();
  }

  template <typename T, Privacy privacy, Instrument instrument>
  absl::Status LogUnSafe(T value,
                         const Definition<T, privacy, instrument>& definition,
                         std::string_view partition) {
    return absl::OkStatus();
  }

  telemetry::BuildDependentConfig& metric_config() { return metric_config_; }

  telemetry::TelemetryConfig config_proto_;
  telemetry::BuildDependentConfig metric_config_;
};

class ContextMapTest : public ::testing::Test {};

class Foo {};

TEST_F(ContextMapTest, GetContext) {
  using TestContextMap = ContextMap<Foo, metric_list_span, TestMetricRouter>;
  Foo foo;
  TestContextMap context_map(std::make_unique<TestMetricRouter>());
  EXPECT_FALSE(context_map.Get(&foo).is_decrypted());

  context_map.Get(&foo).SetDecrypted();
  EXPECT_TRUE(context_map.Get(&foo).is_decrypted());
  CHECK_OK(context_map.Remove(&foo));

  EXPECT_FALSE(context_map.Get(&foo).is_decrypted());
}

constexpr std::string_view pv[] = {"buyer_2", "buyer_1"};
constexpr Definition<int, Privacy::kNonImpacting,
                     Instrument::kPartitionedCounter>
    kIntExactPartitioned("kPartitioned", "description", "buyer_name", pv);
constexpr const DefinitionName* wrong_order_partitioned[] = {
    &kIntExactPartitioned};
constexpr absl::Span<const DefinitionName* const> wrong_order_partitioned_span =
    wrong_order_partitioned;

TEST_F(ContextMapTest, CheckListOrderPartition) {
  EXPECT_DEATH((ContextMap<Foo, wrong_order_partitioned_span, TestMetricRouter>(
                   std::make_unique<TestMetricRouter>())),
               HasSubstr("kPartitioned public partitions"));
}

constexpr double hb[] = {50, 100, 200, 1};
constexpr Definition<int, Privacy::kNonImpacting, Instrument::kHistogram>
    kIntExactHistogram("kHistogram", "description", hb);
constexpr const DefinitionName* wrong_order_histogram[] = {&kIntExactHistogram};
constexpr absl::Span<const DefinitionName* const> wrong_order_histogram_span =
    wrong_order_histogram;

TEST_F(ContextMapTest, CheckListOrderHistogram) {
  EXPECT_DEATH((ContextMap<Foo, wrong_order_histogram_span, TestMetricRouter>(
                   std::make_unique<TestMetricRouter>())),
               HasSubstr("kHistogram histogram"));
}

constexpr std::string_view kDefaultBuyers[] = {"buyer_1", "buyer_2"};
constexpr Definition<int, Privacy::kImpacting, Instrument::kPartitionedCounter>
    metric_1("m_1", "", "partition_type", 1, kDefaultBuyers, 1, 0);
constexpr const DefinitionName* list[] = {&metric_1, &kIntExactCounter};
constexpr absl::Span<const DefinitionName* const> list_span = list;

TEST_F(ContextMapTest, CheckDropNoisyValuesProbability) {
  telemetry::TelemetryConfig config_proto;
  auto metric_config_1 = config_proto.add_metric();
  metric_config_1->set_name("m_1");
  metric_config_1->set_drop_noisy_values_probability(1.0);
  auto config = std::make_unique<telemetry::BuildDependentConfig>(config_proto);
  constexpr PrivacyBudget budget{/*epsilon*/ 5};

  EXPECT_DEATH((GetContextMap<Foo, list_span>(std::move(config), nullptr, "",
                                              "", budget)),
               HasSubstr("m_1 drop_noisy_values_probability is out of range"));
}

TEST_F(ContextMapTest, OnlyCreateOneSafeMetric) {
  using TestContextMap = ContextMap<Foo, metric_list_span, TestMetricRouter>;
  TestContextMap context_map(std::make_unique<TestMetricRouter>());
  TestContextMap::SafeContextT& safe_context = context_map.SafeMetric();
  EXPECT_EQ(&safe_context, &context_map.SafeMetric());
}

constexpr Definition<int, Privacy::kImpacting, Instrument::kUpDownCounter>
    kUnsafe1("kUnsafe1", "", 0, 0);
constexpr Definition<int, Privacy::kImpacting, Instrument::kUpDownCounter>
    kUnsafe2("kUnsafe2", "", 0, 0);
constexpr Definition<int, Privacy::kImpacting, Instrument::kUpDownCounter>
    kUnsafe3("kUnsafe3", "", 0, 0);
constexpr const DefinitionName* unsafe_list[] = {
    &kUnsafe1, &kUnsafe2, &kUnsafe3,        &kCustom1,
    &kCustom2, &kCustom3, &kIntExactCounter};
constexpr absl::Span<const DefinitionName* const> unsafe_list_span =
    unsafe_list;

class MetricConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_proto_.set_mode(telemetry::TelemetryConfig::PROD);
    config_proto_.add_metric()->set_name("kUnsafe1");
    config_proto_.add_metric()->set_name("kUnsafe2");
    config_proto_.add_metric()->set_name("not_defined");
    config_proto_.add_metric()->set_name("placeholder_1");
    config_proto_.add_metric()->set_name("placeholder_2");
    config_proto_.add_metric()->set_name("placeholder_3");
    auto* proto1 = config_proto_.add_custom_udf_metric();
    proto1->set_name("placeholder_1");
    proto1->set_privacy_budget_weight(0.5);
    config_proto_.add_custom_udf_metric()->set_name("placeholder_2");
  }

  telemetry::TelemetryConfig config_proto_;
};

TEST_F(MetricConfigTest, PrivacyBudget) {
  constexpr PrivacyBudget budget{/*epsilon*/ 7};
  auto metric_config =
      std::make_unique<telemetry::BuildDependentConfig>(config_proto_);
  auto c = GetContextMap<Foo, unsafe_list_span>(std::move(metric_config),
                                                nullptr, "", "", budget);
  // privacy budget = epsilon/(CustomMetricsWeight() - CountOfCustomList() +
  // accumulated weight) = 7/(1.5 - 3 + 5)
  EXPECT_DOUBLE_EQ(c->metric_router().dp().privacy_budget_per_weight().epsilon,
                   2);
}

}  // namespace
}  // namespace privacy_sandbox::server_common::metrics
