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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "telemetry.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/sdk/logs/batch_log_record_processor_factory.h"
#include "opentelemetry/sdk/logs/logger.h"
#include "opentelemetry/sdk/logs/logger_provider.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor_factory.h"
#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/metrics/view/view_registry.h"
#include "opentelemetry/sdk/trace/samplers/always_on_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/sdk/version/version.h"
#include "opentelemetry/trace/provider.h"

#include "init.h"
#include "telemetry_provider.h"

namespace logs_api = opentelemetry::logs;
namespace logs_sdk = opentelemetry::sdk::logs;
namespace metric_sdk = opentelemetry::sdk::metrics;
namespace metrics_api = opentelemetry::metrics;
namespace nostd = opentelemetry::nostd;
namespace trace = opentelemetry::trace;
using opentelemetry::sdk::logs::LoggerProviderFactory;
using opentelemetry::sdk::resource::Resource;
using opentelemetry::sdk::resource::ResourceAttributes;
using opentelemetry::sdk::trace::AlwaysOnSamplerFactory;
using opentelemetry::sdk::trace::SimpleSpanProcessorFactory;
using opentelemetry::sdk::trace::TracerProviderFactory;
using opentelemetry::trace::Provider;
using opentelemetry::trace::Scope;
using opentelemetry::trace::Span;
using opentelemetry::trace::StatusCode;
using opentelemetry::trace::Tracer;
using opentelemetry::trace::TracerProvider;

namespace privacy_sandbox::server_common {

void InitTelemetry(std::string service_name, std::string build_version,
                   bool trace_enabled, bool metric_enabled, bool log_enabled) {
  TelemetryProvider::Init(service_name, build_version, trace_enabled,
                          metric_enabled, log_enabled);
}

void ConfigureMetrics(
    Resource resource,
    const metric_sdk::PeriodicExportingMetricReaderOptions& options,
    absl::optional<std::string> collector_endpoint) {
  if (!TelemetryProvider::GetInstance().metric_enabled()) {
    return;
  }
  auto reader =
      CreatePeriodicExportingMetricReader(options, collector_endpoint);
  std::shared_ptr<metrics_api::MeterProvider> provider =
      std::make_shared<metric_sdk::MeterProvider>(
          std::make_unique<metric_sdk::ViewRegistry>(), std::move(resource));
  std::shared_ptr<metric_sdk::MeterProvider> p =
      std::static_pointer_cast<metric_sdk::MeterProvider>(provider);
  p->AddMetricReader(std::move(reader));
  metrics_api::Provider::SetMeterProvider(provider);
}

std::unique_ptr<metrics_api::MeterProvider> ConfigurePrivateMetrics(
    Resource resource,
    const metric_sdk::PeriodicExportingMetricReaderOptions& options,
    absl::optional<std::string> collector_endpoint) {
  if (!TelemetryProvider::GetInstance().metric_enabled()) {
    return std::make_unique<metrics_api::NoopMeterProvider>();
  }
  auto provider = std::make_unique<metric_sdk::MeterProvider>(
      std::make_unique<metric_sdk::ViewRegistry>(), std::move(resource));
  provider->AddMetricReader(
      CreatePeriodicExportingMetricReader(options, collector_endpoint));
  return provider;
}

void ConfigureTracer(Resource resource,
                     absl::optional<std::string> collector_endpoint) {
  if (!TelemetryProvider::GetInstance().trace_enabled()) {
    return;
  }
  auto exporter = CreateSpanExporter(collector_endpoint);
  auto processor = SimpleSpanProcessorFactory::Create(std::move(exporter));
  std::shared_ptr<TracerProvider> provider = TracerProviderFactory::Create(
      std::move(processor), resource, AlwaysOnSamplerFactory::Create(),
      CreateIdGenerator());

  // Set the global trace provider
  Provider::SetTracerProvider(provider);
}

void ConfigureLogger(Resource resource,
                     absl::optional<std::string> collector_endpoint) {
  if (!TelemetryProvider::GetInstance().log_enabled()) {
    return;
  }
  auto exporter = CreateLogRecordExporter(collector_endpoint);
  auto processor =
      logs_sdk::SimpleLogRecordProcessorFactory::Create(std::move(exporter));
  std::shared_ptr<logs_api::LoggerProvider> provider(
      LoggerProviderFactory::Create(std::move(processor), resource));

  // Set the global logger provider
  logs_api::Provider::SetLoggerProvider(provider);
}

std::unique_ptr<logs_api::LoggerProvider> ConfigurePrivateLogger(
    Resource resource, absl::optional<std::string> collector_endpoint) {
  if (!TelemetryProvider::GetInstance().log_enabled()) {
    return std::make_unique<logs_api::NoopLoggerProvider>();
  }
  return LoggerProviderFactory::Create(
      logs_sdk::SimpleLogRecordProcessorFactory::Create(
          CreateLogRecordExporter(collector_endpoint)),
      resource);
}

std::unique_ptr<logs_api::LoggerProvider> ConfigurePrivateBatchLogger(
    opentelemetry::sdk::resource::Resource resource,
    absl::optional<std::string> collector_endpoint,
    const logs_sdk::BatchLogRecordProcessorOptions& options) {
  if (!TelemetryProvider::GetInstance().log_enabled()) {
    return std::make_unique<logs_api::NoopLoggerProvider>();
  }
  return LoggerProviderFactory::Create(
      logs_sdk::BatchLogRecordProcessorFactory::Create(
          CreateLogRecordExporter(collector_endpoint), options),
      resource);
}

nostd::shared_ptr<Tracer> GetTracer() {
  return TelemetryProvider::GetInstance().GetTracer();
}

}  // namespace privacy_sandbox::server_common
