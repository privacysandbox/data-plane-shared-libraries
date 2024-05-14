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

#include <gtest/gtest.h>

#include "absl/base/log_severity.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/scoped_mock_log.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

using google::scp::roma::FunctionBindingPayload;
using google::scp::roma::sandbox::roma_service::RomaService;
using ::testing::_;
using ::testing::AllOf;
using ::testing::HasSubstr;

namespace google::scp::roma::test {
namespace {
constexpr auto kTimeout = absl::Seconds(10);
}

void LoggingFunction(absl::LogSeverity severity,
                     absl::flat_hash_map<std::string, std::string> metadata,
                     std::string_view msg) {
  LOG(LEVEL(severity)) << msg;
}

TEST(LoggingTest, ConsoleLoggingNoOpWhenMinLogLevelSet) {
  Config config;
  config.number_of_workers = 2;
  config.SetLoggingFunction(LoggingFunction);
  RomaService<> roma_service(std::move(config));
  EXPECT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, testing::_, "Hello World"))
      .Times(0);
  EXPECT_CALL(log, Log(absl::LogSeverity::kWarning, testing::_, "Hello World"));
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_, "Hello World"));
  log.StartCapturingLogs();
  {
    const std::string js = R"(
      function Handler() {
        console.log("Hello World");
        console.warn("Hello World");
        console.error("Hello World");
      }
    )";

    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = js,
    });

    EXPECT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   EXPECT_TRUE(resp.ok());
                                   load_finished.Notify();
                                 })
                    .ok());
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .min_log_level = absl::LogSeverity::kWarning,
        });

    EXPECT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               EXPECT_TRUE(resp.ok());
                               if (resp.ok()) {
                                 result = std::move(resp->resp);
                               }
                               execute_finished.Notify();
                             })
                    .ok());
  }
  EXPECT_TRUE(load_finished.WaitForNotificationWithTimeout(kTimeout));
  EXPECT_TRUE(execute_finished.WaitForNotificationWithTimeout(kTimeout));

  EXPECT_TRUE(roma_service.Stop().ok());
  log.StopCapturingLogs();
}

TEST(LoggingTest, StackTracesLoggedWhenLoggingFunctionSet) {
  Config config;
  config.number_of_workers = 2;
  config.SetLoggingFunction(LoggingFunction);
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  absl::Notification load_finished;
  absl::Notification execute_failed;

  absl::ScopedMockLog log;
  constexpr std::string_view input = "Hello World";
  EXPECT_CALL(
      log, Log(absl::LogSeverity::kError, _,
               AllOf(HasSubstr(absl::StrCat("Uncaught Error: ", input)),
                     HasSubstr("at ErrorFunction"), HasSubstr("at Handler"))))
      .Times(2);
  log.StartCapturingLogs();

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
      function ErrorFunction(input) {
        throw new Error(input);
      }

      function Handler(input) {
        ErrorFunction(input);
      }
    )JS_CODE",
    });

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {absl::StrCat("\"", input, "\"")},
        });

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          ASSERT_EQ(resp.status().code(), absl::StatusCode::kInternal);
          execute_failed.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(execute_failed.WaitForNotificationWithTimeout(absl::Seconds(10)));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
  log.StopCapturingLogs();
}

void LogMetadataFunction(absl::LogSeverity severity,
                         absl::flat_hash_map<std::string, std::string> metadata,
                         std::string_view msg) {
  for (const auto& [key, value] : metadata) {
    LOG(LEVEL(severity)) << key << ": " << value;
  }
  LOG(LEVEL(severity)) << msg;
}

TEST(LoggingTest, MetadataInLogsAvailableInBatchedRequests) {
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

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
  }

  load_finished.WaitForNotification();
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

      auto batch_callback =
          [&](const std::vector<absl::StatusOr<ResponseObject>>& batch_resp) {
            for (auto resp : batch_resp) {
              if (resp.ok()) {
                EXPECT_THAT(resp->resp, testing::StrEq("undefined"));
              } else {
                ADD_FAILURE() << "resp is NOT OK.";
              }
            }
            {
              absl::MutexLock lock(&res_count_mu);
              res_count += batch_resp.size();
            }
            local_execute.Notify();
          };
      while (!roma_service.BatchExecute(batch, batch_callback).ok()) {
      }

      // Thread cannot join until batch_callback is called.
      local_execute.WaitForNotification();
    });
  }

  for (auto& t : threads) {
    t.join();
  }
  {
    absl::MutexLock lock(&res_count_mu);
    EXPECT_EQ(res_count, kBatchSize * kNumThreads);
  }

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());

  log.StopCapturingLogs();
}
}  // namespace google::scp::roma::test
