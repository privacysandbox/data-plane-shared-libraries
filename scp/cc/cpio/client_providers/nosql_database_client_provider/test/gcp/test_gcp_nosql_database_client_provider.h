/*
 * Copyright 2023 Google LLC
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

#pragma once

#include <memory>
#include <sstream>
#include <string>

#include "cpio/client_providers/nosql_database_client_provider/src/gcp/gcp_nosql_database_client_provider.h"
#include "google/cloud/options.h"

namespace google::scp::cpio::client_providers {
/// NoSQLDatabaseClientOptions for testing on GCP.
struct TestGcpNoSQLDatabaseClientOptions : public NoSQLDatabaseClientOptions {
  TestGcpNoSQLDatabaseClientOptions() = default;

  explicit TestGcpNoSQLDatabaseClientOptions(
      const NoSQLDatabaseClientOptions& options)
      : NoSQLDatabaseClientOptions(options) {}

  std::string impersonate_service_account;
  virtual ~TestGcpNoSQLDatabaseClientOptions() = default;
};

class TestGcpSpannerFactory : public SpannerFactory {
 public:
  cloud::Options CreateClientOptions(
      std::shared_ptr<NoSQLDatabaseClientOptions> options) noexcept override;

  virtual ~TestGcpSpannerFactory() = default;
};
}  // namespace google::scp::cpio::client_providers
