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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/log/scoped_mock_log.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/config/config.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

using google::scp::roma::FunctionBindingPayload;
using google::scp::roma::sandbox::roma_service::RomaService;
using ::testing::_;
using ::testing::StrEq;

namespace google::scp::roma::test {

void LogMetadataFunction(FunctionBindingPayload<>& wrapper) {
  for (const auto& [key, val] : wrapper.metadata) {
    LOG(INFO) << key << ": " << val;
  }
}

template <typename T>
std::unique_ptr<FunctionBindingObjectV2<T>> CreateFunctionBindingObject(
    std::function<void(FunctionBindingPayload<T>&)> func,
    std::string_view name) {
  auto function_binding_object = std::make_unique<FunctionBindingObjectV2<T>>();
  function_binding_object->function = func;
  function_binding_object->function_name = name;
  return function_binding_object;
}

std::unique_ptr<FunctionBindingObjectV2<>> CreateLogFunctionBindingObject() {
  return CreateFunctionBindingObject<
      absl::flat_hash_map<std::string, std::string>>(LogMetadataFunction,
                                                     "log_metadata");
}

TEST(MetadataSapiTest, MetadataAssociatedWithEachNativeFunction) {
  Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(CreateLogFunctionBindingObject());
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  absl::Notification load_finished;
  size_t total_runs = 10;
  std::vector<std::string> results(total_runs);
  std::vector<absl::Notification> finished(total_runs);
  std::vector<absl::Status> response_statuses(total_runs);
  const auto& metadata_tag = "Working";

  absl::ScopedMockLog log;
  for (auto i = 0u; i < total_runs; i++) {
    EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, _,
                         absl::StrCat("key", i, ": ", metadata_tag, i)));
  }
  log.StartCapturingLogs();

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = "var Handler = () => log_metadata();",
    });

    absl::Status response;
    ASSERT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(response.ok());
  }

  {
    for (auto i = 0u; i < total_runs; ++i) {
      auto code_obj =
          std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
              .id = "foo",
              .version_string = "v1",
              .handler_name = "Handler",
          });
      code_obj->metadata.insert(
          {absl::StrCat("key", i), absl::StrCat(metadata_tag, i)});

      ASSERT_TRUE(roma_service
                      .Execute(std::move(code_obj),
                               [&, i](absl::StatusOr<ResponseObject> resp) {
                                 response_statuses[i] = resp.status();
                                 if (resp.ok()) {
                                   results[i] = resp->resp;
                                 }
                                 finished[i].Notify();
                               })
                      .ok());
    }
  }

  for (auto i = 0u; i < total_runs; ++i) {
    finished[i].WaitForNotificationWithTimeout(absl::Seconds(30));
    ASSERT_TRUE(response_statuses[i].ok());
    EXPECT_THAT(results[i], testing::StrEq("undefined"));
  }

  ASSERT_TRUE(roma_service.Stop().ok());

  log.StopCapturingLogs();
}

TEST(MetadataSapiTest, MetadataAssociatedWithBatchedFunctions) {
  Config config;
  config.worker_queue_max_items = 1;
  config.number_of_workers = 10;
  config.RegisterFunctionBinding(CreateLogFunctionBindingObject());
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  absl::Notification load_finished;
  constexpr int kNumThreads = 10;
  constexpr size_t kBatchSize = 100;
  const auto& metadata_tag = "Working";

  absl::ScopedMockLog log;
  for (auto i = 0; i < kNumThreads; i++) {
    for (int j = 0; j < kBatchSize; j++) {
      EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, _,
                           absl::StrCat("key", i, ": ", metadata_tag, j)));
    }
  }
  log.StartCapturingLogs();

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = "var Handler = () => log_metadata();",
    });

    absl::Status response;
    ASSERT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    EXPECT_TRUE(response.ok());
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
        if (resp.ok()) {
          EXPECT_THAT(resp->resp, testing::StrEq("undefined"));
        } else {
          ADD_FAILURE() << "resp is NOT OK.";
        }
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
