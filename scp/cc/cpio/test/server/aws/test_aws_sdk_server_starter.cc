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

#include "test_aws_sdk_server_starter.h"

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "core/test/utils/aws_helper/aws_helper.h"
#include "core/test/utils/docker_helper/docker_helper.h"
#include "cpio/server/interface/configuration_keys.h"
#include "cpio/server/src/blob_storage_service/test_aws/test_configuration_keys.h"
#include "cpio/server/src/metric_service/test_aws/test_configuration_keys.h"
#include "cpio/server/src/parameter_service/test_aws/test_configuration_keys.h"

using Aws::Map;
using Aws::String;
using google::scp::core::test::StartLocalStackContainer;

namespace google::scp::cpio::test {
void TestAwsSdkServerStarter::RunCloud() {
  // Starts localstack.
  if (StartLocalStackContainer(config_.network_name,
                               config_.cloud_container_name,
                               config_.cloud_port) != 0) {
    throw std::runtime_error("Failed to start localstack container!");
  }
}

absl::btree_map<std::string, std::string>
TestAwsSdkServerStarter::CreateSdkEnvVariables() {
  std::string cloud_endpoint_in_container = absl::StrCat(
      "http://", config_.cloud_container_name, ":", config_.cloud_port);
  return {
      {kSdkClientLogOption, "ConsoleLog"},
      {kTestMetricClientCloudEndpointOverride, cloud_endpoint_in_container},
      {kTestBlobStorageClientCloudEndpointOverride,
       cloud_endpoint_in_container},
      {kTestParameterClientCloudEndpointOverride, cloud_endpoint_in_container},
  };
}
}  // namespace google::scp::cpio::test
