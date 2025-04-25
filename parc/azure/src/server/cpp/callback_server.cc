// Copyright 2024 Google LLC
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

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include <grpcpp/ext/otel_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "opentelemetry/exporters/otlp/otlp_file_client_options.h"
#include "opentelemetry/exporters/otlp/otlp_file_metric_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_file_metric_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_options.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/aggregation/default_aggregation.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h"
#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/metrics/meter_context_factory.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "src/server/cpp/callback_server_impl.h"

ABSL_FLAG(std::string, address, "0.0.0.0", "IP address of PARC gRPC server.");
ABSL_FLAG(int64_t, port, 51337, "Port of PARC gRPC server.");
ABSL_FLAG(bool, verbose, false, "Log level INFO to stderr.");
ABSL_FLAG(std::string, parameters_file_path, "",
          "Path to file containing parameters");
ABSL_FLAG(std::optional<std::string>, metrics_file, std::nullopt,
          "Destination file for server metrics.");
ABSL_FLAG(std::optional<std::string>, otel_collector, std::nullopt,
          "host:port for the Otel collector.");
ABSL_FLAG(bool, use_workload_auth, false,
          "Determines authentication method for storage client. Use true for "
          "Workload Identity auth. If false, must provide storage account "
          "name and key");
ABSL_FLAG(std::string, account_name, "devstoreaccount1", "Azure account name.");
ABSL_FLAG(std::string, account_key,
          "Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/"
          "K1SZFPTOtr/KBHBeksoGMGw==",
          "Azure account key.");
ABSL_FLAG(std::string, blob_http_endpoint, "",
          "URL for http endpoint from which to retrieve blobs, e.g. "
          "https://mystorageaccount.blob.core.windows.net");
ABSL_FLAG(int64_t, blob_chunk_size, 4'000'000,
          "Size of retrieved blob range retrieved.");
ABSL_FLAG(int64_t, chunk_buffer_size, 3,
          "Number of blob chunks to buffer for each GetBlob request.");
ABSL_FLAG(std::string, certs_dir, "/etc/ssl/certs",
          "Path to dir containing CA certificates");
namespace {

namespace otlp_exporter = opentelemetry::exporter::otlp;
namespace otlp_metrics_sdk = opentelemetry::sdk::metrics;

const absl::Duration kExportDuration = absl::Seconds(1);
const absl::Duration kExportTimeoutDuration = absl::Milliseconds(200);

absl::Status InitMetrics() {
  std::unique_ptr<otlp_metrics_sdk::PushMetricExporter> exporter;
  if (absl::GetFlag(FLAGS_otel_collector).has_value()) {
    otlp_exporter::OtlpGrpcMetricExporterOptions exporter_opts;
    exporter_opts.endpoint =
        std::string(absl::GetFlag(FLAGS_otel_collector).value());
    exporter_opts.use_ssl_credentials = false;
    exporter = otlp_exporter::OtlpGrpcMetricExporterFactory::Create(
        std::move(exporter_opts));
  } else if (absl::GetFlag(FLAGS_metrics_file).has_value()) {
    otlp_exporter::OtlpFileClientFileSystemOptions fs_backend;
    fs_backend.file_pattern =
        std::string(absl::GetFlag(FLAGS_metrics_file).value());
    otlp_exporter::OtlpFileMetricExporterOptions exporter_opts;
    exporter_opts.backend_options = fs_backend;
    exporter = otlp_exporter::OtlpFileMetricExporterFactory::Create(
        std::move(exporter_opts));
  }
  if (exporter) {
    // Initialize and set the global MeterProvider
    otlp_metrics_sdk::PeriodicExportingMetricReaderOptions reader_opts;
    reader_opts.export_interval_millis =
        std::chrono::milliseconds(absl::ToInt64Milliseconds(kExportDuration));
    reader_opts.export_timeout_millis = std::chrono::milliseconds(
        absl::ToInt64Milliseconds(kExportTimeoutDuration));

    auto reader =
        otlp_metrics_sdk::PeriodicExportingMetricReaderFactory::Create(
            std::move(exporter), std::move(reader_opts));
    auto context = otlp_metrics_sdk::MeterContextFactory::Create();
    context->AddMetricReader(std::move(reader));
    auto meter_provider =
        otlp_metrics_sdk::MeterProviderFactory::Create(std::move(context));
    // std::shared_ptr<otlp_metrics_sdk::MeterProvider> provider(
    //     std::move(meter_provider));
    // opentelemetry::metrics::Provider::SetMeterProvider(provider);
    if (auto status = grpc::OpenTelemetryPluginBuilder()
                          .SetMeterProvider(std::move(meter_provider))
                          .BuildAndRegisterGlobal();
        !status.ok()) {
      return status;
    }
  }
  return absl::OkStatus();
}

void CleanupMetrics() {
  std::shared_ptr<otlp_metrics_sdk::MeterProvider> none;
  opentelemetry::metrics::Provider::SetMeterProvider(none);
}

absl::Status RunCallbackServer() {
  const int64_t server_port = absl::GetFlag(FLAGS_port);
  const std::string server_address =
      absl::StrCat(absl::GetFlag(FLAGS_address), ":", server_port);
  const std::filesystem::path parameters_file_path(
      absl::GetFlag(FLAGS_parameters_file_path));

  LOG(INFO) << "Initializing the PARC gRPC Azure server...";
  auto parc_server = privacysandbox::parc::azure::CallbackServerImpl::Create(
      parameters_file_path, absl::GetFlag(FLAGS_use_workload_auth),
      absl::GetFlag(FLAGS_account_name), absl::GetFlag(FLAGS_account_key),
      absl::GetFlag(FLAGS_blob_http_endpoint), absl::GetFlag(FLAGS_certs_dir),
      absl::GetFlag(FLAGS_blob_chunk_size),
      absl::GetFlag(FLAGS_chunk_buffer_size));

  if (!parc_server.ok()) {
    return parc_server.status();
  } else if (!(*parc_server)) {
    return absl::InternalError("Could not create server");
  }

  grpc::EnableDefaultHealthCheckService(true);
  grpc::ServerBuilder builder;
  int selected_port;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials(),
                           &selected_port);

  builder.RegisterService(parc_server->get());
  std::unique_ptr<grpc::Server> grpc_server = builder.BuildAndStart();
  if (grpc_server == nullptr) {
    return absl::UnavailableError(absl::StrCat(
        "Failed to start gRPC server on address: ", server_address));
  }
  LOG(INFO) << "PARC gRPC server listening at: " << server_address;
  if (selected_port != server_port) {
    return absl::ResourceExhaustedError(absl::StrCat(
        absl::StrCat("Port does not match requested port: ", server_port)));
  }
  grpc_server->Wait();
  return absl::OkStatus();
}

}  // namespace

int main(int argc, char* argv[]) {
  // The first thing we do is make sure that crashes will have a stacktrace
  // printed, with demangled symbols.
  absl::InitializeSymbolizer(argv[0]);
  {
    absl::FailureSignalHandlerOptions options;
    absl::InstallFailureSignalHandler(options);
  }
  absl::InitializeLog();
  // TODO - Write a program usage message.
  // absl::SetProgramUsageMessage(kUsageMessage);
  absl::ParseCommandLine(argc, argv);
  absl::SetStderrThreshold(absl::GetFlag(FLAGS_verbose)
                               ? absl::LogSeverity::kInfo
                               : absl::LogSeverity::kWarning);
  if (absl::Status status = InitMetrics(); !status.ok()) {
    LOG(ERROR) << status;
    return 1;
  }
  if (absl::Status status = RunCallbackServer(); !status.ok()) {
    LOG(ERROR) << status;
    return 1;
  }
  CleanupMetrics();
  return 0;
}
