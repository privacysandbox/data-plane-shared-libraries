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

#ifndef METRIC_DEFINITION_H_
#define METRIC_DEFINITION_H_

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/types/span.h"

// Defines metric `Definition`, and a list of common metrics.
namespace privacy_sandbox::server_common::metrics {

enum class Privacy { kNonImpacting, kImpacting };

enum class Instrument {
  kUpDownCounter,
  kPartitionedCounter,
  kHistogram,
  kGauge
};

inline constexpr std::array<double, 0> kEmptyHistogramBoundaries = {};
inline constexpr std::array<std::string_view, 0> kEmptyPublicPartition = {};
inline constexpr std::string_view kEmptyPartitionType;
inline constexpr std::string_view kNoiseAttribute = "Noise";
inline constexpr std::string_view kGenerationIdAttribute = "generation_id";

struct DefinitionName {
  constexpr explicit DefinitionName(std::string_view name,
                                    std::string_view description)
      : name_(name), description_(description) {}

  std::string_view name_;
  std::string_view description_;
  absl::Span<const std::string_view> public_partitions_copy_;
  absl::Span<const double> histogram_boundaries_copy_;
  double privacy_budget_weight_copy_ = 0;
};

namespace internal {
struct Partitioned {
  constexpr explicit Partitioned(
      std::string_view partition_type = kEmptyPartitionType,
      int max_partitions_contributed = 1,
      absl::Span<const std::string_view> public_partitions =
          kEmptyPublicPartition)
      : partition_type_(partition_type),
        max_partitions_contributed_(max_partitions_contributed),
        public_partitions_(public_partitions) {}

  std::string_view partition_type_;
  int max_partitions_contributed_;
  absl::Span<const std::string_view> public_partitions_;  // must be sorted
};

struct Histogram {
  constexpr explicit Histogram(
      absl::Span<const double> histogram_boundaries = kEmptyHistogramBoundaries)
      : histogram_boundaries_(histogram_boundaries) {}

  absl::Span<const double> histogram_boundaries_;  // must be sorted
};

template <typename T>
struct DifferentialPrivacy {
  constexpr explicit DifferentialPrivacy(
      T upper_bound = std::numeric_limits<T>::max(),
      T lower_bound = std::numeric_limits<T>::min(),
      double drop_noisy_values_probability = 0.0,
      double privacy_budget_weight = 1.0)
      : upper_bound_(std::max(lower_bound, upper_bound)),
        lower_bound_(std::min(lower_bound, upper_bound)),
        drop_noisy_values_probability_(drop_noisy_values_probability),
        privacy_budget_weight_(privacy_budget_weight) {}

  T upper_bound_;
  T lower_bound_;
  // The probability that noisy values will be turned to 0.
  // Setting this to a higher value ensures that the noisy values will be turned
  // to 0, but also means that some actual (non-noisy values) may also be turned
  // to 0. Setting this to 1 means all values will be turned to 0 (eliminating
  // all noise, but also eliminating all useful data)
  double drop_noisy_values_probability_;
  // All Privacy kImpacting metrics split total privacy budget based on their
  // weight. i.e. privacy_budget = total_budget * privacy_budget_weight_ /
  // total_weight
  double privacy_budget_weight_;
};
}  // namespace internal

// `T` can be int or double
// Examples to create `Definition` of different `Privacy` and `Instrument`
//
// * UpDownCounter (non-impacting and impacting)
// Definition<int, Privacy::kNonImpacting, Instrument::kUpDownCounter>
// d1(/*name=*/"d1", /*description==*/"d11");
//
// Definition<int, Privacy::kImpacting, Instrument::kUpDownCounter> d5(
//     /*name=*/"d5", /*description=*/"d51", /*upper_bound=*/9, /*lower_bound=*/
//     1);
//
// * PartitionedCounter (non-impacting and impacting)
//   -- use kEmptyPublicPartition for non-public partition.
// std::string_view public_partitions[] = {"buyer_1", "buyer_2"};
// Definition<int, Privacy::kNonImpacting, Instrument::kPartitionedCounter> d2(
//     /*name=*/"d2", /*description=*/"d21" /*partition_type=*/"buyer_name",
//     /*public_partitions=*/public_partitions);
//
// Definition<int, Privacy::kImpacting, Instrument::kPartitionedCounter> d6(
//     /*name=*/"d6", /*description=*/"d61", /*partition_type=*/"buyer_name",
//     /*max_partitions_contributed=*/2,
//     /*public_partitions=*/public_partitions,
//     /*upper_bound=*/9,
//     /*lower_bound=*/1);
//
// * Histogram (non-impacting and impacting)
// double histogram_boundaries[] = {1, 2};
// Definition<int, Privacy::kNonImpacting, Instrument::kHistogram> d3(
//     /*name=*/"d3", /*description=*/"d31", /*histogram_boundaries=*/hb);
//
// Definition<int, Privacy::kImpacting, Instrument::kHistogram> d7(
//     /*name=*/"d7", /*description=*/"d71", /*histogram_boundaries=*/hb,
//     /*upper_bound=*/9, /*lower_bound=*/0);
//
// * Gauge (non-impacting only)
// Definition<int, Privacy::kNonImpacting, Instrument::kGauge> d4(/*name=*/"d4",
// /*description*/"d41");
//
// The definition pointers should then be added into a list, which defines the
// list of metrics the server can log. A Span of the list is used to initialize
// 'Context'. For example:
//   const DefinitionName* metric_list[] = {&d1, &d2, &d3, &d4, &d5, &d6, &d7};
//   absl::Span<const DefinitionName* const> metric_list_span = metric_list;
//
template <typename T, Privacy privacy, Instrument instrument>
struct Definition : DefinitionName,
                    internal::Partitioned,
                    internal::Histogram,
                    internal::DifferentialPrivacy<T> {
  static_assert(std::is_same<T, int>::value || std::is_same<T, double>::value,
                "T must be int or double");
  using TypeT = T;
  Privacy type_privacy = privacy;
  Instrument type_instrument = instrument;

  using internal::DifferentialPrivacy<T>::upper_bound_;
  using internal::DifferentialPrivacy<T>::lower_bound_;
  using internal::DifferentialPrivacy<T>::privacy_budget_weight_;

  std::string DebugString() const {
    std::string_view instrument_name = "unknown";
    switch (instrument) {
      case Instrument::kUpDownCounter:
        instrument_name = "UpDownCounter";
        break;
      case Instrument::kPartitionedCounter:
        instrument_name = "Partitioned UpDownCounter";
        break;
      case Instrument::kHistogram:
        instrument_name = "Histogram";
        break;
      case Instrument::kGauge:
        instrument_name = "Gauge";
        break;
      default:
        instrument_name = absl::StrCat("instrument [", instrument, "] unknown");
    }
    return absl::Substitute(
        "$0 $1($9) $2 histogram[$3]\n partition by'$4' "
        "max_partitions_contributed:$8 "
        "partition_value[$5]\n bound[$7 ~ $6]",
        name_,
        privacy == Privacy::kNonImpacting ? "Privacy NonImpacting"
                                          : "Privacy Impacting",
        instrument_name, absl::StrJoin(histogram_boundaries_, ","),
        partition_type_, absl::StrJoin(public_partitions_, ","), upper_bound_,
        lower_bound_, max_partitions_contributed_, privacy_budget_weight_);
  }

  template <Privacy non_impact = privacy, Instrument counter = instrument>
  constexpr explicit Definition(
      std::string_view name, std::string_view description,
      std::enable_if_t<non_impact == Privacy::kNonImpacting &&
                       counter == Instrument::kUpDownCounter>* = nullptr)
      : DefinitionName(name, description) {}

  template <Instrument counter = instrument>
  constexpr explicit Definition(
      std::string_view name, std::string_view description, T upper_bound,
      T lower_bound,
      std::enable_if_t<counter == Instrument::kUpDownCounter>* = nullptr)
      : DefinitionName(name, description),
        internal::DifferentialPrivacy<T>(upper_bound, lower_bound) {
    privacy_budget_weight_copy_ = privacy_budget_weight_;
  }

  template <Privacy non_impact = privacy,
            Instrument partitioned_counter = instrument>
  constexpr explicit Definition(
      std::string_view name, std::string_view description,
      std::string_view partition_type,
      absl::Span<const std::string_view> public_partitions,
      std::enable_if_t<non_impact == Privacy::kNonImpacting &&
                       partitioned_counter ==
                           Instrument::kPartitionedCounter>* = nullptr)
      : DefinitionName(name, description),
        internal::Partitioned(partition_type, INT_MAX, public_partitions) {
    public_partitions_copy_ = public_partitions_;
  }

  template <Instrument partitioned_counter = instrument>
  constexpr explicit Definition(
      std::string_view name, std::string_view description,
      std::string_view partition_type, int max_partitions_contributed,
      absl::Span<const std::string_view> public_partitions, T upper_bound,
      T lower_bound, double drop_noisy_values_probability = 0.0,
      std::enable_if_t<partitioned_counter ==
                       Instrument::kPartitionedCounter>* = nullptr)
      : DefinitionName(name, description),
        internal::Partitioned(partition_type, max_partitions_contributed,
                              public_partitions),
        internal::DifferentialPrivacy<T>(upper_bound, lower_bound,
                                         drop_noisy_values_probability) {
    public_partitions_copy_ = public_partitions_;
    privacy_budget_weight_copy_ = privacy_budget_weight_;
  }

  template <Privacy non_impact = privacy, Instrument histogram = instrument>
  constexpr explicit Definition(
      std::string_view name, std::string_view description,
      absl::Span<const double> histogram_boundaries,
      std::enable_if_t<non_impact == Privacy::kNonImpacting &&
                       histogram == Instrument::kHistogram>* = nullptr)
      : DefinitionName(name, description),
        internal::Histogram(histogram_boundaries) {
    histogram_boundaries_copy_ = histogram_boundaries_;
  }

  template <Privacy impact = privacy, Instrument histogram = instrument>
  constexpr explicit Definition(
      std::string_view name, std::string_view description,
      absl::Span<const double> histogram_boundaries, T upper_bound,
      T lower_bound,
      std::enable_if_t<impact == Privacy::kImpacting &&
                       histogram == Instrument::kHistogram>* = nullptr)
      : DefinitionName(name, description),
        internal::Histogram(histogram_boundaries),
        internal::DifferentialPrivacy<T>(upper_bound, lower_bound) {
    histogram_boundaries_copy_ = histogram_boundaries_;
    privacy_budget_weight_copy_ = privacy_budget_weight_;
  }

  template <Privacy non_impact = privacy, Instrument gauge = instrument>
  constexpr explicit Definition(
      std::string_view name, std::string_view description,
      std::enable_if_t<non_impact == Privacy::kNonImpacting &&
                       gauge == Instrument::kGauge>* = nullptr)
      : DefinitionName(name, description) {}
};

// Checks if a Definition in a list
template <typename T, Privacy privacy, Instrument instrument>
constexpr bool IsInList(const Definition<T, privacy, instrument>& definition,
                        absl::Span<const DefinitionName* const> metric_list) {
  for (const auto* m : metric_list) {
    if (&definition == m) {
      return true;
    }
  }
  return false;
}

// List of common metrics used by any servers
inline constexpr Definition<int, Privacy::kNonImpacting,
                            Instrument::kUpDownCounter>
    kTotalRequestFailedCount(
        "request.failed_count",
        "Total number of requests that resulted in failure");

inline constexpr Definition<int, Privacy::kNonImpacting,
                            Instrument::kUpDownCounter>
    kTotalRequestCount("request.count",
                       "Total number of requests received by the server");

inline constexpr double kTimeHistogram[] = {
    1,    2,    3,    4,    6,     9,     13,    18,    25,   35,   50,
    71,   100,  141,  200,  283,   400,   566,   800,   1131, 1600, 2263,
    3200, 4525, 6400, 9051, 12800, 18102, 25600, 36204, 51200};
inline constexpr Definition<int, Privacy::kNonImpacting, Instrument::kHistogram>
    kServerTotalTimeMs("request.duration_ms",
                       "Total time taken by the server to execute the request",
                       kTimeHistogram);

inline constexpr double kSizeHistogram[] = {
    250,    500,     1'000,   2'000,   4'000,     8'000,     16'000,   32'000,
    64'000, 128'000, 256'000, 512'000, 1'024'000, 2'048'000, 4'096'000};
inline constexpr Definition<int, Privacy::kNonImpacting, Instrument::kHistogram>
    kResponseByte("response.size_bytes", "Response size in bytes",
                  kSizeHistogram);

inline constexpr Definition<int, Privacy::kNonImpacting, Instrument::kHistogram>
    kRequestByte("request.size_bytes", "Request size in bytes", kSizeHistogram);

inline constexpr Definition<double, Privacy::kNonImpacting, Instrument::kGauge>
    kCpuPercent("system.cpu.percent", "cpu usage");
inline constexpr Definition<double, Privacy::kNonImpacting, Instrument::kGauge>
    kMemoryKB("system.memory.usage_kb", "Memory usage");
inline constexpr Definition<double, Privacy::kNonImpacting, Instrument::kGauge>
    kKeyFetchFailureCount(
        "system.key_fetch.failure_count",
        "failure counts for fetching keys with the coordinator");
inline constexpr Definition<double, Privacy::kNonImpacting, Instrument::kGauge>
    kNumPrivateKeysFetched(
        "system.key_fetch.num_private_keys_fetched",
        "Number of private keys in response from Private Key Service");
inline constexpr Definition<double, Privacy::kNonImpacting, Instrument::kGauge>
    kNumKeysParsed("system.key_fetch.num_keys_parsed",
                   "Number of keys parsed after the most recent key fetch");
inline constexpr Definition<double, Privacy::kNonImpacting, Instrument::kGauge>
    kNumKeysCached("system.key_fetch.num_keys_cached",
                   "Number of keys currently cached in memory "
                   "after the most recent key fetch");

inline constexpr Definition<int, Privacy::kNonImpacting,
                            Instrument::kUpDownCounter>
    kInitiatedRequestCount("initiated_request.count",
                           "Total number of requests initiated by the server");
inline constexpr Definition<int, Privacy::kNonImpacting, Instrument::kHistogram>
    kInitiatedRequestTotalDuration(
        "initiated_request.duration_ms",
        "Total duration of requests initiated by the server", kTimeHistogram);
inline constexpr Definition<int, Privacy::kNonImpacting, Instrument::kHistogram>
    kInitiatedRequestByte("initiated_request.total_size_bytes",
                          "Size of all the initiated requests by a server",
                          kSizeHistogram);
inline constexpr Definition<int, Privacy::kNonImpacting, Instrument::kHistogram>
    kInitiatedResponseByte(
        "initiated_response.total_size_bytes",
        "Size of all the initiated responses received by a server",
        kSizeHistogram);
inline constexpr Definition<int, Privacy::kNonImpacting,
                            Instrument::kUpDownCounter>
    kInitiatedRequestErrorCount("initiated_request.errors_count",
                                "Total number of errors occurred for the "
                                "requests received by the server");

inline constexpr Definition<double, Privacy::kImpacting,
                            Instrument::kPartitionedCounter>
    kCustom1("placeholder_1", "No. of times metric 1 return udf request",
             kEmptyPartitionType, 1, kEmptyPublicPartition, 1, 0);
inline constexpr Definition<double, Privacy::kImpacting,
                            Instrument::kPartitionedCounter>
    kCustom2("placeholder_2", "No. of times metric 2 return udf request",
             kEmptyPartitionType, 1, kEmptyPublicPartition, 1, 0);
inline constexpr Definition<double, Privacy::kImpacting,
                            Instrument::kPartitionedCounter>
    kCustom3("placeholder_3", "No. of times metric 3 return udf request",
             kEmptyPartitionType, 1, kEmptyPublicPartition, 1, 0);

inline constexpr const Definition<double, Privacy::kImpacting,
                                  Instrument::kPartitionedCounter>*
    kCustomList[] = {
        &kCustom1,
        &kCustom2,
        &kCustom3,
};

constexpr int CountOfCustomList() {
  return std::end(kCustomList) - std::begin(kCustomList);
}

}  // namespace privacy_sandbox::server_common::metrics

#endif  // METRIC_DEFINITION_H_
