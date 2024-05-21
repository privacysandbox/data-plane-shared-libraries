
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
#include "test_aws_utils.h"

#include <memory>
#include <string>

#include <aws/core/Aws.h>
#include <aws/core/client/ClientConfiguration.h>

using Aws::Client::ClientConfiguration;

namespace {
/// Fixed connect timeout to create an AWS client.
constexpr int kConnectTimeoutMs = 3000;
/// Fixed request timeout to create an AWS client.
constexpr int kRequestTimeoutMs = 6000;
}  // namespace

namespace google::scp::cpio::common::test {
ClientConfiguration CreateTestClientConfiguration(
    std::string_view endpoint_override, std::string_view region) noexcept {
  ClientConfiguration config;
  if (!endpoint_override.empty()) {
    config.endpointOverride = endpoint_override;
  }

  if (!region.empty()) {
    config.region = region;
  }

  config.scheme = Aws::Http::Scheme::HTTPS;
  config.connectTimeoutMs = kConnectTimeoutMs;
  config.requestTimeoutMs = kRequestTimeoutMs;
  return config;
}
}  // namespace google::scp::cpio::common::test
