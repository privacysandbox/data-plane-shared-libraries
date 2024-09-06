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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
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

TEST(MetadataTest, InvocationReqMetadataVisibleInNativeFunction) {
  Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(CreateLogFunctionBindingObject());
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;
  const std::pair<std::string, std::string> metadata_pair{"key1", "val1"};

  absl::ScopedMockLog log;
  EXPECT_CALL(
      log, Log(absl::LogSeverity::kInfo, _,
               absl::StrCat(metadata_pair.first, ": ", metadata_pair.second)));
  log.StartCapturingLogs();

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = "var Handler = () => log_metadata();",
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
        });
    execution_obj->metadata.insert(metadata_pair);

    status = roma_service.Execute(std::move(execution_obj),
                                  [&](absl::StatusOr<ResponseObject> resp) {
                                    EXPECT_TRUE(resp.ok());
                                    if (resp.ok()) {
                                      result = std::move(resp->resp);
                                    }
                                    execute_finished.Notify();
                                  });
    EXPECT_TRUE(status.ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_THAT(result, testing::StrEq("undefined"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
  log.StopCapturingLogs();
}

TEST(MetadataTest, MetadataAssociatedWithEachNativeFunction) {
  Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(CreateLogFunctionBindingObject());
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  absl::Notification load_finished;
  size_t total_runs = 10;
  std::vector<std::string> results(total_runs);
  std::vector<absl::Notification> finished(total_runs);
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

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
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

      status = roma_service.Execute(
          std::move(code_obj), [&, i](absl::StatusOr<ResponseObject> resp) {
            EXPECT_TRUE(resp.ok());
            if (resp.ok()) {
              results[i] = resp->resp;
            }
            finished[i].Notify();
          });
      EXPECT_TRUE(status.ok());
    }
  }

  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));

  for (auto i = 0u; i < total_runs; ++i) {
    finished[i].WaitForNotificationWithTimeout(absl::Seconds(30));
    EXPECT_THAT(results[i], testing::StrEq("undefined"));
  }

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());

  log.StopCapturingLogs();
}

TEST(MetadataTest, MetadataAssociatedWithBatchedFunctions) {
  Config config;
  config.worker_queue_max_items = 1;
  config.number_of_workers = 10;
  config.RegisterFunctionBinding(CreateLogFunctionBindingObject());
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

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

void LogMetadataStringFunction(FunctionBindingPayload<std::string>& wrapper) {
  LOG(INFO) << wrapper.metadata;
}

TEST(MetadataTest, StringMetadataVisibleInNativeFunction) {
  Config<std::string> config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(CreateFunctionBindingObject<std::string>(
      LogMetadataStringFunction, "log_metadata"));
  RomaService<std::string> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;
  const auto& metadata_tag = "Working";

  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, _, metadata_tag));
  log.StartCapturingLogs();

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = "var Handler = () => log_metadata();",
    });

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<std::string>>(
        InvocationStrRequest<std::string>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .metadata = metadata_tag,
        });

    status = roma_service.Execute(std::move(execution_obj),
                                  [&](absl::StatusOr<ResponseObject> resp) {
                                    EXPECT_TRUE(resp.ok());
                                    if (resp.ok()) {
                                      result = std::move(resp->resp);
                                    }
                                    execute_finished.Notify();
                                  });
    EXPECT_TRUE(status.ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_THAT(result, testing::StrEq("undefined"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
  log.StopCapturingLogs();
}

void LogMetadataVectorFunction(
    FunctionBindingPayload<std::vector<std::string>>& wrapper) {
  for (const auto& metadata : wrapper.metadata) {
    LOG(INFO) << metadata;
  }
}

TEST(MetadataTest, VectorMetadataVisibleInNativeFunction) {
  Config<std::vector<std::string>> config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(
      CreateFunctionBindingObject<std::vector<std::string>>(
          LogMetadataVectorFunction, "log_metadata"));
  RomaService<std::vector<std::string>> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;
  std::vector<std::string> metadata_list(5, "Working");

  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, _, metadata_list[0]))
      .Times(metadata_list.size());
  log.StartCapturingLogs();

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = "var Handler = () => log_metadata();",
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
        std::make_unique<InvocationStrRequest<std::vector<std::string>>>(
            InvocationStrRequest<std::vector<std::string>>{
                .id = "foo",
                .version_string = "v1",
                .handler_name = "Handler",
                .metadata = metadata_list,
            });

    status = roma_service.Execute(std::move(execution_obj),
                                  [&](absl::StatusOr<ResponseObject> resp) {
                                    EXPECT_TRUE(resp.ok());
                                    if (resp.ok()) {
                                      result = std::move(resp->resp);
                                    }
                                    execute_finished.Notify();
                                  });
    EXPECT_TRUE(status.ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_THAT(result, testing::StrEq("undefined"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
  log.StopCapturingLogs();
}

struct Metadata {
  std::string name;
  std::string description;
  std::vector<std::string> tags;
};

void LogMetadataStructFunction(
    FunctionBindingPayload<std::vector<Metadata>>& wrapper) {
  for (const auto& metadata : wrapper.metadata) {
    LOG(INFO) << metadata.name;
    LOG(INFO) << metadata.description;
    for (const auto& tag : metadata.tags) {
      LOG(INFO) << tag;
    }
  }
}

TEST(MetadataTest, CustomMetadataTypeVisibleInNativeFunction) {
  Config<std::vector<Metadata>> config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(
      CreateFunctionBindingObject<std::vector<Metadata>>(
          LogMetadataStructFunction, "log_metadata"));
  RomaService<std::vector<Metadata>> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  const auto& metadata_factory = [](int i) {
    Metadata metadata = {absl::StrCat("foo", i), absl::StrCat("bar", i), {}};
    for (int j = 0; j < 10; j++) {
      metadata.tags.push_back(absl::StrCat("hello", i, "world", j));
    }
    return metadata;
  };
  std::vector<Metadata> metadata_list;
  for (int i = 0; i < 10; i++) {
    metadata_list.push_back(metadata_factory(i));
  }

  absl::ScopedMockLog log;
  for (const auto& metadata : metadata_list) {
    EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, _, metadata.name));
    EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, _, metadata.description));
    for (const auto& tag : metadata.tags) {
      EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, _, tag));
    }
  }
  log.StartCapturingLogs();

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = "var Handler = () => log_metadata();",
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
        std::make_unique<InvocationStrRequest<std::vector<Metadata>>>(
            InvocationStrRequest<std::vector<Metadata>>{
                .id = "foo",
                .version_string = "v1",
                .handler_name = "Handler",
            });
    execution_obj->metadata = metadata_list;

    status = roma_service.Execute(std::move(execution_obj),
                                  [&](absl::StatusOr<ResponseObject> resp) {
                                    EXPECT_TRUE(resp.ok());
                                    if (resp.ok()) {
                                      result = std::move(resp->resp);
                                    }
                                    execute_finished.Notify();
                                  });
    EXPECT_TRUE(status.ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_THAT(result, testing::StrEq("undefined"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
  log.StopCapturingLogs();
}

class MoveOnly {
 public:
  std::string data_;

  explicit MoveOnly(std::string data) : data_(std::move(data)) {}

  MoveOnly(MoveOnly&& other) : data_(std::move(other.data_)) {
    other.data_ = "";
  }

  MoveOnly& operator=(MoveOnly&& other) {
    if (this != &other) {
      data_ = other.data_;
      other.data_ = "";
    }
    return *this;
  }

  ~MoveOnly() {}
};

TEST(MetadataTest, MoveOnlyClassCannotBeCopied) {
  EXPECT_FALSE(std::is_copy_constructible<MoveOnly>::value);
}

void LogMetadataMoveOnlyFunction(FunctionBindingPayload<MoveOnly>& wrapper) {
  LOG(INFO) << wrapper.metadata.data_;
}

TEST(MetadataTest, MoveOnlyMetadataVisibleInNativeFunction) {
  Config<MoveOnly> config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(CreateFunctionBindingObject<MoveOnly>(
      LogMetadataMoveOnlyFunction, "log_metadata"));
  RomaService<MoveOnly> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;
  const auto& metadata_tag = "Working";

  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, _, metadata_tag));
  log.StartCapturingLogs();

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = "var Handler = () => log_metadata();",
    });

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<MoveOnly>>(
        InvocationStrRequest<MoveOnly>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .metadata = MoveOnly(metadata_tag),
        });

    status = roma_service.Execute(std::move(execution_obj),
                                  [&](absl::StatusOr<ResponseObject> resp) {
                                    EXPECT_TRUE(resp.ok());
                                    if (resp.ok()) {
                                      result = resp->resp;
                                    }
                                    execute_finished.Notify();
                                  });
    EXPECT_TRUE(status.ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_THAT(result, testing::StrEq("undefined"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
  log.StopCapturingLogs();
}

TEST(MetadataTest, MoveOnlyMetadataVisibleInAllNativeFunctions) {
  Config<MoveOnly> config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(CreateFunctionBindingObject<MoveOnly>(
      LogMetadataMoveOnlyFunction, "log_metadata"));
  RomaService<MoveOnly> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;
  const auto& metadata_tag = "Working";

  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, _, metadata_tag)).Times(2);
  log.StartCapturingLogs();

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"(var Handler = () => {
            log_metadata();
            log_metadata();
        })",
    });

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<MoveOnly>>(
        InvocationStrRequest<MoveOnly>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .metadata = MoveOnly(metadata_tag),
        });

    status = roma_service.Execute(std::move(execution_obj),
                                  [&](absl::StatusOr<ResponseObject> resp) {
                                    EXPECT_TRUE(resp.ok());
                                    if (resp.ok()) {
                                      result = resp->resp;
                                    }
                                    execute_finished.Notify();
                                  });
    EXPECT_TRUE(status.ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_THAT(result, testing::StrEq("undefined"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
  log.StopCapturingLogs();
}

}  // namespace google::scp::roma::test
