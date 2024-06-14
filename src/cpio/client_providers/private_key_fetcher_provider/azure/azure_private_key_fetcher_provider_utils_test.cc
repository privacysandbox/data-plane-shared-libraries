/*
 * Portions Copyright (c) Microsoft Corporation
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

#include "azure_private_key_fetcher_provider_utils.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "src/core/interface/http_types.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::scp::core::ExecutionResult;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;

namespace {
constexpr char kKeyId[] = "123";
constexpr char kPrivateKeyBaseUri[] = "http://localhost.test:8000";
}  // namespace

namespace google::scp::cpio::client_providers::test {

TEST(AzurePrivateKeyFetchingClientUtilsTest, CreateHttpRequest) {
  PrivateKeyFetchingRequest request;
  request.key_vending_endpoint = std::make_shared<PrivateKeyVendingEndpoint>();
  request.key_vending_endpoint->private_key_vending_service_endpoint =
      kPrivateKeyBaseUri;
  request.max_age_seconds = 1000000;
  HttpRequest http_request;
  AzurePrivateKeyFetchingClientUtils::CreateHttpRequest(request, http_request);

  EXPECT_EQ(http_request.method, HttpMethod::POST);
  EXPECT_EQ(*http_request.path, std::string(kPrivateKeyBaseUri));
}

}  // namespace google::scp::cpio::client_providers::test
