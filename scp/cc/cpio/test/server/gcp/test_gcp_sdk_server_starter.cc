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

#include "test_gcp_sdk_server_starter.h"

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "core/test/utils/docker_helper/docker_helper.h"
#include "cpio/server/interface/configuration_keys.h"
#include "cpio/server/interface/queue_service/configuration_keys.h"
#include "cpio/server/src/queue_service/test_gcp/test_configuration_keys.h"

using google::scp::core::test::GetIpAddress;
using google::scp::core::test::StartGcpContainer;

namespace google::scp::cpio::test {
void TestGcpSdkServerStarter::RunCloud() {
  // Starts GCP container.
  if (StartGcpContainer(config_.network_name, config_.cloud_container_name,
                        config_.cloud_port) != 0) {
    throw std::runtime_error("Failed to start GCP container!");
  }

  // Needs to start PubSub Emulator separately.
  if (StartPubSubEmulator() != 0) {
    throw std::runtime_error("Failed to start Pubsub emulator!");
  }
}

absl::btree_map<std::string, std::string>
TestGcpSdkServerStarter::CreateSdkEnvVariables() {
  return absl::btree_map<std::string, std::string>({
      {kSdkClientLogOption, "ConsoleLog"},
      // gcp_endpoint_in_container
      {kTestGcpQueueClientCloudEndpointOverride,
       GetIpAddress(config_.network_name, config_.cloud_container_name) + ":" +
           config_.cloud_port},
      // "test-project" is the pre-exist project in the emulator.
      {kTestGcpQueueClientProjectId, "test-project"},
      {kQueueClientQueueName, config_.queue_service_queue_name},
  });
}

int TestGcpSdkServerStarter::StartPubSubEmulator() {
  std::string command =
      absl::StrCat("docker exec -itd ", config_.cloud_container_name,
                   " gcloud beta emulators pubsub start --host-port 0.0.0.0:",
                   config_.cloud_port);
  return std::system(command.c_str());
}
}  // namespace google::scp::cpio::test
