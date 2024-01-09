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

#include "roma/sandbox/worker_pool/src/worker_pool_api_sapi.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "public/core/test/interface/execution_result_matchers.h"
#include "roma/sandbox/worker_api/src/worker_api_sapi.h"

using google::scp::roma::sandbox::worker_api::WorkerApiSapi;
using google::scp::roma::sandbox::worker_api::WorkerApiSapiConfig;

namespace {
const WorkerApiSapiConfig worker_config = {
    .js_engine_require_code_preload = true,
    .compilation_context_cache_size = 5,
    .native_js_function_comms_fd = -1,
    .native_js_function_names = {},
    .max_worker_virtual_memory_mb = 0,
    .sandbox_request_response_shared_buffer_size_mb = 0,
    .enable_sandbox_sharing_request_response_with_buffer_only = false,
};
}  // namespace

namespace google::scp::roma::sandbox::worker_pool::test {
TEST(WorkerPoolTest, CanInitRunAndStop) {
  constexpr int num_workers = 4;
  std::vector<WorkerApiSapiConfig> configs(num_workers, worker_config);
  auto pool = WorkerPoolApiSapi(configs);

  ASSERT_TRUE(pool.Init().ok());
  ASSERT_TRUE(pool.Run().ok());
  EXPECT_TRUE(pool.Stop().ok());
}

TEST(WorkerPoolTest, CanGetPoolCount) {
  constexpr int num_workers = 2;
  std::vector<WorkerApiSapiConfig> configs(num_workers, worker_config);
  auto pool = WorkerPoolApiSapi(configs);

  ASSERT_TRUE(pool.Init().ok());
  ASSERT_TRUE(pool.Run().ok());
  EXPECT_EQ(pool.GetPoolSize(), num_workers);
  EXPECT_TRUE(pool.Stop().ok());
}

TEST(WorkerPoolTest, CanGetWorker) {
  int num_workers = 2;
  std::vector<WorkerApiSapiConfig> configs;
  for (int i = 0; i < num_workers; i++) {
    configs.push_back(worker_config);
  }

  auto pool = WorkerPoolApiSapi(configs);

  ASSERT_TRUE(pool.Init().ok());
  ASSERT_TRUE(pool.Run().ok());

  auto worker1 = pool.GetWorker(0);
  ASSERT_TRUE(worker1.ok());
  auto worker2 = pool.GetWorker(1);
  ASSERT_TRUE(worker2.ok());

  EXPECT_NE(worker1, worker2);
  EXPECT_TRUE(pool.Stop().ok());
}

}  // namespace google::scp::roma::sandbox::worker_pool::test
