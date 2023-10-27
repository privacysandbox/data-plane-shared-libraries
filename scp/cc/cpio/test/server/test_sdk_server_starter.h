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

#ifndef CPIO_TEST_SERVER_TEST_SDK_SERVER_STARTER_H_
#define CPIO_TEST_SERVER_TEST_SDK_SERVER_STARTER_H_

#include <string>

#include "absl/container/btree_map.h"

namespace google::scp::cpio::test {

struct TestSdkServerConfig {
  std::string region;
  std::string network_name;

  std::string cloud_container_name;
  std::string cloud_port;

  std::string sdk_container_name;
  std::string sdk_port;

  std::string job_service_queue_name;
  std::string job_service_table_name;
  std::string queue_service_queue_name;
};

class TestSdkServerStarter {
 public:
  explicit TestSdkServerStarter(const TestSdkServerConfig& config)
      : config_(config) {}

  void Setup();

  virtual void RunCloud() = 0;

  void RunSdkServer(
      const std::string& image_location, const std::string& image_name,
      const absl::btree_map<std::string, std::string>& env_overrides = {});

  void StopSdkServer();

  void Teardown();

 protected:
  TestSdkServerConfig config_;

 private:
  virtual absl::btree_map<std::string, std::string> CreateSdkEnvVariables() = 0;
};
}  // namespace google::scp::cpio::test

#endif  // CPIO_TEST_SERVER_TEST_SDK_SERVER_STARTER_H_
