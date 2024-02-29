/*
 * Copyright 2022 Google LLC
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

#ifndef COMPONENTS_TELEMETRY_TELEMETRY_H_
#define COMPONENTS_TELEMETRY_TELEMETRY_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/logs/provider.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/logs/batch_log_record_processor_options.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/trace/tracer.h"

namespace privacy_sandbox::server_common {

// Must be called to initialize telemetry functionality.
void InitTelemetry(std::string service_name, std::string build_version,
                   bool trace_enabled = true, bool metric_enabled = true,
                   bool log_enabled = true);

// Initialize metrics functionality. meter provider is shared through OTel api.
// If `ConfigureMetrics` is not called, all metrics recording will be NoOp.
[[deprecated]] void ConfigureMetrics(
    opentelemetry::sdk::resource::Resource resource,
    const opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions&
        options,
    absl::optional<std::string> collector_endpoint = absl::nullopt);

// Must be called to initialize metrics functionality.
// If `ConfigurePrivateMetrics` is not called, all metrics recording will be
// NoOp. Returned MetricReader is not shared, invoker takes ownership.
std::unique_ptr<opentelemetry::metrics::MeterProvider> ConfigurePrivateMetrics(
    opentelemetry::sdk::resource::Resource resource,
    const opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions&
        options,
    absl::optional<std::string> collector_endpoint = absl::nullopt);

// Must be called to initialize tracing functionality.
// If `ConfigureTracer` is not called, all tracing will be NoOp.
void ConfigureTracer(
    opentelemetry::sdk::resource::Resource resource,
    absl::optional<std::string> collector_endpoint = absl::nullopt);

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> GetTracer();

// Must be called to initialize logging functionality.
// If `ConfigureLogger` is not called, all logging will be NoOp.
[[deprecated]] void ConfigureLogger(
    opentelemetry::sdk::resource::Resource resource,
    absl::optional<std::string> collector_endpoint = absl::nullopt);

// Must be called to initialize log functionality.
// If `ConfigurePrivateLogger` is not called, all log recording will be
// NoOp. Returned LoggerProvider is not shared, invoker takes ownership.
std::unique_ptr<opentelemetry::logs::LoggerProvider> ConfigurePrivateLogger(
    opentelemetry::sdk::resource::Resource resource,
    absl::optional<std::string> collector_endpoint);

// Same as above, but use `BatchLogRecordProcessor` to improve performance
std::unique_ptr<opentelemetry::logs::LoggerProvider>
ConfigurePrivateBatchLogger(
    opentelemetry::sdk::resource::Resource resource,
    absl::optional<std::string> collector_endpoint,
    const opentelemetry::sdk::logs::BatchLogRecordProcessorOptions& options);

}  // namespace privacy_sandbox::server_common

#endif  // COMPONENTS_TELEMETRY_TELEMETRY_H_
