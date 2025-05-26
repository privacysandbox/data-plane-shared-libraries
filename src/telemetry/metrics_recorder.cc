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

#include "metrics_recorder.h"

#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "glog/logging.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/trace/tracer.h"

namespace metric_sdk = opentelemetry::sdk::metrics;

namespace privacy_sandbox::server_common {
namespace {

using opentelemetry::sdk::metrics::MeterSelector;

// TODO(b/278899152): Get both library and schema versions updated in one place.
constexpr std::string_view kSchema = "https://opentelemetry.io/schemas/1.20.0";
// The units below are nanoseconds.
constexpr double kDefaultHistogramBuckets[] = {
    40'000,        80'000,      120'000,     160'000,       220'000,
    280'000,       320'000,     640'000,     1'200'000,     2'500'000,
    5'000'000,     10'000'000,  20'000'000,  40'000'000,    80'000'000,
    160'000'000,   320'000'000, 640'000'000, 1'300'000'000, 2'600'000'000,
    5'000'000'000,
};

class NoopMetricsRecorder : public MetricsRecorder {
 public:
  ~NoopMetricsRecorder() override = default;

  void IncrementEventStatus(std::string event, absl::Status status,
                            uint64_t count = 1) override {}

  void IncrementEventCounter(std::string event) override {}

  void RegisterHistogram(std::string event, std::string description,
                         std::string unit,
                         std::vector<double> bucket_boundaries = {}) override {}

  void RecordHistogramEvent(std::string event, int64_t value) override {}

  void RecordLatency(std::string event, absl::Duration duration) override {}

  void SetCommonLabel(std::string label, std::string label_value) override {}
};

class MetricsRecorderImpl : public MetricsRecorder {
 public:
  MetricsRecorderImpl(std::string service_name, std::string build_version)
      : service_name_(std::move(service_name)),
        build_version_(std::move(build_version)) {
    auto meter = GetMeter();
    event_count_ =
        meter->CreateUInt64Counter("EventCount", "Count of named events.");
    event_status_count_ = meter->CreateUInt64Counter(
        "EventStatus", "Count of status code associated with events.");

    // Catch-all histogram, with preconfigured buckets which should work for
    // most latencies distribution.
    RegisterHistogramView("Latency", "Latency View", "ns", {});
    latency_histogram_ = meter->CreateUInt64Histogram(
        "Latency", "Histogram of latencies associated with events.",
        "nanosecond");
  }

  ~MetricsRecorderImpl() override = default;

  void IncrementEventStatus(std::string event, absl::Status status,
                            uint64_t count = 1) override {
    absl::flat_hash_map<std::string, std::string> labels = common_labels_;
    labels.insert({"event", std::move(event)});
    labels.insert({"status", absl::StatusCodeToString(status.code())});
    const auto labelkv =
        opentelemetry::common::KeyValueIterableView<decltype(labels)>{labels};
    event_status_count_->Add(count, labelkv);
  }

  void RecordHistogramEvent(std::string event, int64_t value) override {
    auto context = opentelemetry::context::RuntimeContext::GetCurrent();
    absl::ReaderMutexLock lock(&mutex_);
    const auto key_iter = histograms_.find(event);

    if (key_iter == histograms_.end()) {
      LOG(ERROR) << "The following histogram hasn't been initialized: "
                 << event;
    }
    absl::flat_hash_map<std::string, std::string> labels = common_labels_;
    labels.insert({"event", std::move(event)});
    const auto labelkv =
        opentelemetry::common::KeyValueIterableView<decltype(labels)>{labels};
    key_iter->second->Record(value, labelkv, context);
  }

  void RecordLatency(std::string event, absl::Duration duration) override {
    absl::flat_hash_map<std::string, std::string> labels = common_labels_;
    labels.insert({"event", std::move(event)});
    const auto labelkv =
        opentelemetry::common::KeyValueIterableView<decltype(labels)>{labels};
    auto context = opentelemetry::context::RuntimeContext::GetCurrent();
    latency_histogram_->Record(absl::ToInt64Nanoseconds(duration), labelkv,
                               context);
  }

  void IncrementEventCounter(std::string event) override {
    absl::flat_hash_map<std::string, std::string> labels = common_labels_;
    labels.insert({"event", std::move(event)});
    const auto labelkv =
        opentelemetry::common::KeyValueIterableView<decltype(labels)>{labels};
    event_count_->Add(1, labelkv);
  }

  void RegisterHistogram(std::string event, std::string description,
                         std::string unit,
                         std::vector<double> bucket_boundaries) override {
    absl::MutexLock lock(&mutex_);
    if (const auto key_iter = histograms_.find(event);
        key_iter != histograms_.end()) {
      return;
    }
    RegisterHistogramView(event, description, unit, bucket_boundaries);
    auto meter = GetMeter();
    auto histogram = meter->CreateUInt64Histogram(event, description, unit);
    histograms_.insert_or_assign(std::move(event), std::move(histogram));
  }

  void SetCommonLabel(std::string label, std::string label_value) override {
    common_labels_.insert({std::move(label), std::move(label_value)});
  }

 private:
  void RegisterHistogramView(const std::string& name,
                             const std::string& description,
                             const std::string& unit,
                             std::vector<double> bucket_boundaries) {
    if (bucket_boundaries.empty()) {
      bucket_boundaries.insert(bucket_boundaries.begin(),
                               std::begin(kDefaultHistogramBuckets),
                               std::end(kDefaultHistogramBuckets));
    }
    auto provider = opentelemetry::metrics::Provider::GetMeterProvider();
    // TODO: b/277098640 - Remove this dynamic cast.
    auto sdk_provider =
        dynamic_cast<metric_sdk::MeterProvider*>(provider.get());
    if (!sdk_provider) {
      LOG(ERROR) << "Unable to cast meter provider from "
                    "nostd::shared_ptr<MeterProvider>";
      return;
    }
    auto histogram_instrument_selector =
        std::make_unique<metric_sdk::InstrumentSelector>(
            metric_sdk::InstrumentType::kHistogram, name, unit);
    auto histogram_meter_selector = GetMeterSelector(std::string{kSchema});
    auto histogram_aggregation_config =
        std::make_shared<metric_sdk::HistogramAggregationConfig>();
    histogram_aggregation_config->boundaries_ = bucket_boundaries;
    auto histogram_view = std::make_unique<metric_sdk::View>(
        name, description, unit, metric_sdk::AggregationType::kHistogram,
        std::move(histogram_aggregation_config));
    sdk_provider->AddView(std::move(histogram_instrument_selector),
                          std::move(histogram_meter_selector),
                          std::move(histogram_view));
  }

  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> GetMeter()
      const {
    auto provider = opentelemetry::metrics::Provider::GetMeterProvider();
    return provider->GetMeter(service_name_, build_version_);
  }

  std::unique_ptr<MeterSelector> GetMeterSelector(std::string selector) const {
    return std::make_unique<MeterSelector>(service_name_, build_version_,
                                           selector);
  }

  opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>>
      event_status_count_;
  opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Histogram<uint64_t>>
      latency_histogram_;
  opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<uint64_t>>
      event_count_;
  mutable absl::Mutex mutex_;
  absl::flat_hash_map<std::string,
                      opentelemetry::nostd::unique_ptr<
                          opentelemetry::metrics::Histogram<uint64_t>>>
      histograms_ ABSL_GUARDED_BY(mutex_);
  std::string service_name_;
  std::string build_version_;
  absl::flat_hash_map<std::string, std::string> common_labels_;
};

}  // namespace

ScopeLatencyRecorder::ScopeLatencyRecorder(std::string event_name,
                                           MetricsRecorder& metrics_recorder)
    : stop_watch_(Stopwatch()),
      event_name_(std::move(event_name)),
      metrics_recorder_(metrics_recorder) {}

ScopeLatencyRecorder::~ScopeLatencyRecorder() {
  metrics_recorder_.RecordLatency(event_name_, GetLatency());
}

absl::Duration ScopeLatencyRecorder::GetLatency() {
  return stop_watch_.GetElapsedTime();
}

std::unique_ptr<MetricsRecorder> MetricsRecorder::Create(
    std::string service_name, std::string build_version) {
  return std::make_unique<MetricsRecorderImpl>(std::move(service_name),
                                               std::move(build_version));
}

std::unique_ptr<MetricsRecorder> MetricsRecorder::CreateNoop() {
  return std::make_unique<NoopMetricsRecorder>();
}

}  // namespace privacy_sandbox::server_common
