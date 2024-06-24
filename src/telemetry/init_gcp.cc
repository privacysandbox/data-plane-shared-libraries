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

#include "opentelemetry/exporters/otlp/otlp_grpc_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h"
#include "opentelemetry/sdk/trace/random_id_generator_factory.h"

#include "init.h"

namespace privacy_sandbox::server_common {
std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> CreateSpanExporter(
    absl::optional<std::string> collector_endpoint) {
  opentelemetry::exporter::otlp::OtlpGrpcExporterOptions opts;
  if (collector_endpoint.has_value()) {
    opts.endpoint = *collector_endpoint;
  }
  return opentelemetry::exporter::otlp::OtlpGrpcExporterFactory::Create(opts);
}

std::unique_ptr<opentelemetry::sdk::metrics::MetricReader>
CreatePeriodicExportingMetricReader(
    const opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions&
        reader_options,
    absl::optional<std::string> collector_endpoint) {
  opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions exporter_options;
  if (collector_endpoint.has_value()) {
    exporter_options.endpoint = *collector_endpoint;
  }
  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter =
      opentelemetry::exporter::otlp::OtlpGrpcMetricExporterFactory::Create(
          exporter_options);
  return std::make_unique<
      opentelemetry::sdk::metrics::PeriodicExportingMetricReader>(
      std::move(exporter), reader_options);
}

std::unique_ptr<opentelemetry::sdk::trace::IdGenerator> CreateIdGenerator() {
  return opentelemetry::sdk::trace::RandomIdGeneratorFactory::Create();
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>
CreateLogRecordExporter(absl::optional<std::string> collector_endpoint) {
  opentelemetry::exporter::otlp::OtlpGrpcLogRecordExporterOptions opts;
  if (collector_endpoint.has_value()) {
    opts.endpoint = *collector_endpoint;
  }
  return opentelemetry::exporter::otlp::OtlpGrpcLogRecordExporterFactory::
      Create(opts);
}

}  // namespace privacy_sandbox::server_common
