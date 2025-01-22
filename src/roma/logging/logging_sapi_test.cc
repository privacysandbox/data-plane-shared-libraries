/*
 * Copyright 2024 Google LLC
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

#include <gtest/gtest.h>

#include "absl/base/log_severity.h"
#include "absl/log/log.h"
#include "absl/log/scoped_mock_log.h"
#include "absl/status/status.h"
#include "absl/synchronization/notification.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

using google::scp::roma::sandbox::roma_service::RomaService;
using ::testing::_;

namespace google::scp::roma::test {
void LogMetadataFunction(absl::LogSeverity severity,
                         absl::flat_hash_map<std::string, std::string> metadata,
                         std::string_view msg) {
  for (const auto& [key, value] : metadata) {
    LOG(LEVEL(severity)) << key << ": " << value;
  }
  LOG(LEVEL(severity)) << msg;
}

TEST(LoggingSapiTest, MetadataInLogsAvailableInBatchedRequests) {
  Config config;
  config.worker_queue_max_items = 1;
  config.number_of_workers = 10;
  config.SetLoggingFunction(LogMetadataFunction);
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  absl::Notification load_finished;
  constexpr int kNumThreads = 10;
  constexpr size_t kBatchSize = 100;
  const auto& metadata_tag = "Working";

  absl::ScopedMockLog log;
  for (int i = 0; i < kNumThreads; i++) {
    for (int j = 0; j < kBatchSize; j++) {
      EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, _,
                           absl::StrCat("key", i, ": ", metadata_tag, j)));
    }
  }
  EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, _, metadata_tag))
      .Times(kBatchSize * kNumThreads);
  log.StartCapturingLogs();

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = "var Handler = (input) => console.log(input);",
    });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response_status = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(response_status.ok());
  }

  absl::Mutex res_count_mu;
  int res_count = 0;

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  for (int i = 0; i < kNumThreads; i++) {
    threads.emplace_back([&, i]() {
      absl::Notification local_execute;
      std::vector<InvocationStrRequest<>> batch;
      for (int j = 0; j < kBatchSize; j++) {
        InvocationStrRequest<> execution_obj{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {absl::StrCat("\"", metadata_tag, "\"")},
            .metadata = {{absl::StrCat("key", i),
                          absl::StrCat(metadata_tag, j)}},
        };
        batch.push_back(execution_obj);
      }

      std::vector<absl::StatusOr<ResponseObject>> batch_responses;
      auto batch_callback =
          [&](const std::vector<absl::StatusOr<ResponseObject>>& batch_resp) {
            batch_responses = batch_resp;
            {
              absl::MutexLock lock(&res_count_mu);
              res_count += batch_resp.size();
            }
            local_execute.Notify();
          };
      // Each retry needs to be done with a copy of the batch to give each
      // request its own metadata, as BatchExecute moves metadata from each
      // request in the batch to MetadataStorage.
      for (auto batch_copy = batch;
           !roma_service.BatchExecute(batch_copy, batch_callback).ok();
           batch_copy = batch) {
      }

      // Thread cannot join until batch_callback is called.
      ASSERT_TRUE(
          local_execute.WaitForNotificationWithTimeout(absl::Seconds(10)));

      for (auto resp : batch_responses) {
        EXPECT_TRUE(resp.ok()) << "resp is NOT OK.";
        EXPECT_THAT(resp->resp, testing::StrEq("\"\""));
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }
  {
    absl::MutexLock lock(&res_count_mu);
    EXPECT_EQ(res_count, kBatchSize * kNumThreads);
  }

  ASSERT_TRUE(roma_service.Stop().ok());

  log.StopCapturingLogs();
}
}  // namespace google::scp::roma::test
