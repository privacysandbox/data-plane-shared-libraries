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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "opentelemetry/metrics/provider.h"

#include "telemetry_provider.h"

namespace privacy_sandbox::server_common {

namespace {

TEST(Init, WithoutTraceOrMetric) {
  InitTelemetry("service_name", "build_version",
                /*trace_enabled=*/false,
                /*metric_enabled=*/false);
  opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions options;
  auto resource = opentelemetry::sdk::resource::Resource::GetDefault();
  ConfigureMetrics(resource, options);
  ConfigureTracer(resource);

  EXPECT_TRUE(
      dynamic_cast<opentelemetry::trace::NoopTracer*>(GetTracer().get()));
  EXPECT_TRUE(dynamic_cast<opentelemetry::metrics::NoopMeterProvider*>(
      opentelemetry::metrics::Provider::GetMeterProvider().get()));
}

}  // namespace

}  // namespace privacy_sandbox::server_common
