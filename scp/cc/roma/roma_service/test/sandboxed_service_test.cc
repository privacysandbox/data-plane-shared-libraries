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

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/log/log.h"
#include "absl/log/scoped_mock_log.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "roma/config/src/config.h"
#include "roma/config/src/function_binding_object_v2.h"
#include "roma/interface/roma.h"
#include "roma/sandbox/roma_service/src/roma_service.h"
#include "roma/wasm/test/testing_utils.h"
#include "src/cpp/util/duration.h"

using google::scp::roma::FunctionBindingPayload;
using google::scp::roma::sandbox::roma_service::RomaService;
using google::scp::roma::wasm::testing::WasmTestingUtils;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::StrEq;

namespace google::scp::roma::test {
static const std::vector<uint8_t> kWasmBin = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
    0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
    0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
    0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b,
};

// TODO(b/311435456): Reenable when cause of flakiness found.
TEST(SandboxedServiceTest, DISABLED_InitStop) {
  auto roma_service = std::make_unique<RomaService<>>();
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());
  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest,
     ShouldFailToInitializeIfVirtualMemoryCapIsTooLittle) {
  Config config;
  config.max_worker_virtual_memory_mb = 10;

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.message(),
              StrEq("Roma initialization failed due to internal "
                    "error: Could not initialize "
                    "the wrapper API."));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ExecuteCode) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("Foobar")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ExecuteCodeWithStringViewInput) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto load_code_obj_request = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
            function Handler(input) { return "Hello world! " + JSON.stringify(input);
          }
        )JS_CODE",
    });

    status = roma_service->LoadCodeObj(
        std::move(load_code_obj_request),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    std::string_view input_str_view{R"("Foobar")"};
    auto execute_request =
        std::make_unique<InvocationStrViewRequest<>>(InvocationStrViewRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {input_str_view},
        });

    status = roma_service->Execute(
        std::move(execute_request),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ExecuteNativeLogFunctions) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  const auto& input = R"("Foobar")";
  const auto& trim_first_last_char = [](const std::string& str) {
    return str.substr(1, str.length() - 2);
  };

  absl::ScopedMockLog log;
  EXPECT_CALL(log,
              Log(absl::LogSeverity::kInfo, _, trim_first_last_char(input)));
  EXPECT_CALL(log,
              Log(absl::LogSeverity::kWarning, _, trim_first_last_char(input)));
  EXPECT_CALL(log,
              Log(absl::LogSeverity::kError, _, trim_first_last_char(input)));
  log.StartCapturingLogs();

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) {
      roma.n_log(input);
      roma.n_warn(input);
      roma.n_error(input);
      return `Hello world! ${input}`;
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(input);

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result,
              testing::StrEq(absl::StrCat(R"("Hello world! )",
                                          trim_first_last_char(input), "\"")));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());

  log.StopCapturingLogs();
}

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

std::unique_ptr<
    FunctionBindingObjectV2<absl::flat_hash_map<std::string, std::string>>>
CreateLogFunctionBindingObject() {
  return CreateFunctionBindingObject<
      absl::flat_hash_map<std::string, std::string>>(LogMetadataFunction,
                                                     "log_metadata");
}

TEST(SandboxedServiceTest, InvocationReqMetadataVisibleInNativeFunctions) {
  Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(CreateLogFunctionBindingObject());
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

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

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
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

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, testing::StrEq("undefined"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
  log.StopCapturingLogs();
}

TEST(SandboxedServiceTest, MetadataAssociatedWithEachNativeFunction) {
  Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(CreateLogFunctionBindingObject());
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

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

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
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

      status = roma_service->Execute(
          std::move(code_obj),
          [&, i](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
            EXPECT_TRUE(resp->ok());
            if (resp->ok()) {
              auto& code_resp = **resp;
              results[i] = code_resp.resp;
            }
            finished[i].Notify();
          });
      EXPECT_TRUE(status.ok());
    }
  }

  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  for (auto i = 0u; i < total_runs; ++i) {
    finished[i].WaitForNotificationWithTimeout(absl::Seconds(30));
    EXPECT_THAT(results[i], testing::StrEq("undefined"));
  }

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());

  log.StopCapturingLogs();
}

TEST(SandboxedServiceTest, MetadataAssociatedWithBatchedFunctions) {
  Config config;
  config.worker_queue_max_items = 1;
  config.number_of_workers = 10;
  config.RegisterFunctionBinding(CreateLogFunctionBindingObject());
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  absl::Notification load_finished;
  constexpr int kNumThreads = 10;
  constexpr size_t kBatchSize = 100;
  const auto& metadata_tag = "Working";

  absl::ScopedMockLog log;
  for (auto i = 0u; i < kNumThreads; i++) {
    EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, _,
                         absl::StrCat("key", i, ": ", metadata_tag, i)))
        .Times(kBatchSize);
  }
  log.StartCapturingLogs();

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = "var Handler = () => log_metadata();",
    });

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  load_finished.WaitForNotification();
  absl::Mutex res_count_mu;
  int res_count ABSL_GUARDED_BY(res_count_mu) = 0;

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  for (int i = 0; i < kNumThreads; i++) {
    threads.emplace_back([&, i]() {
      absl::Notification local_execute;
      InvocationStrRequest<> execution_obj{
          .id = "foo",
          .version_string = "v1",
          .handler_name = "Handler",
      };
      execution_obj.metadata.insert(
          {absl::StrCat("key", i), absl::StrCat(metadata_tag, i)});

      std::vector<InvocationStrRequest<>> batch(kBatchSize, execution_obj);

      auto batch_callback =
          [&,
           i](const std::vector<absl::StatusOr<ResponseObject>>& batch_resp) {
            for (auto resp : batch_resp) {
              if (resp.ok()) {
                EXPECT_THAT(resp->resp, testing::StrEq("undefined"));
              } else {
                ADD_FAILURE() << "resp is NOT OK.";
              }
            }
            {
              absl::MutexLock l(&res_count_mu);
              res_count += batch_resp.size();
            }
            local_execute.Notify();
          };
      while (!roma_service->BatchExecute(batch, batch_callback).ok()) {}

      // Thread cannot join until batch_callback is called.
      local_execute.WaitForNotification();
    });
  }

  for (auto& t : threads) {
    t.join();
  }
  {
    absl::MutexLock l(&res_count_mu);
    EXPECT_EQ(res_count, kBatchSize * kNumThreads);
  }

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());

  log.StopCapturingLogs();
}

void LogMetadataStringFunction(FunctionBindingPayload<std::string>& wrapper) {
  LOG(INFO) << wrapper.metadata;
}

TEST(SandboxedServiceTest, StringMetadataVisibleInNativeFunctions) {
  Config<std::string> config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(CreateFunctionBindingObject<std::string>(
      LogMetadataStringFunction, "log_metadata"));
  auto roma_service = std::make_unique<RomaService<std::string>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

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

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
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

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, testing::StrEq("undefined"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
  log.StopCapturingLogs();
}

void LogMetadataVectorFunction(
    FunctionBindingPayload<std::vector<std::string>>& wrapper) {
  for (const auto& metadata : wrapper.metadata) {
    LOG(INFO) << metadata;
  }
}

TEST(SandboxedServiceTest, VectorMetadataVisibleInNativeFunctions) {
  Config<std::vector<std::string>> config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(
      CreateFunctionBindingObject<std::vector<std::string>>(
          LogMetadataVectorFunction, "log_metadata"));
  auto roma_service =
      std::make_unique<RomaService<std::vector<std::string>>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

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

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
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

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, testing::StrEq("undefined"));

  status = roma_service->Stop();
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

TEST(SandboxedServiceTest, CustomMetadataTypeVisibleInNativeFunctions) {
  Config<std::vector<Metadata>> config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(
      CreateFunctionBindingObject<std::vector<Metadata>>(
          LogMetadataStructFunction, "log_metadata"));
  auto roma_service =
      std::make_unique<RomaService<std::vector<Metadata>>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

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

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
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
                .metadata = metadata_list,
            });

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, testing::StrEq("undefined"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
  log.StopCapturingLogs();
}

TEST(SandboxedServiceTest, ShouldFailWithInvalidHandlerName) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;
  absl::Notification failed_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("Foobar")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "WrongHandler";
    execution_obj->input.push_back(R"("Foobar")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          // Execute should fail with the expected error.
          EXPECT_FALSE(resp->ok());
          EXPECT_THAT(resp->status().message(),
                      StrEq("Failed to get valid function handler."));
          failed_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  failed_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ExecuteCodeWithEmptyId) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("Foobar")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldAllowEmptyInputs) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(arg1, arg2) { return arg1; }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, StrEq("undefined"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldGetIdInResponse) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->id = "my_cool_id";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          EXPECT_THAT((*resp)->id, StrEq("my_cool_id"));
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("Foobar")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest,
     ShouldReturnWithVersionNotFoundWhenExecutingAVersionThatHasNotBeenLoaded) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  // We don't load any code, just try to execute some version
  absl::Notification execute_finished;

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("Foobar")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          // Execute should fail with the expected error.
          EXPECT_FALSE(resp->ok());
          EXPECT_THAT(resp->status().message(),
                      StrEq("Could not find code version in cache."));
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, CanRunAsyncJsCode) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
      function sleep(milliseconds) {
        const date = Date.now();
        let currentDate = null;
        do {
          currentDate = Date.now();
        } while (currentDate - date < milliseconds);
      }

      function multiplePromises() {
        const p1 = Promise.resolve("some");
        const p2 = "cool";
        const p3 = new Promise((resolve, reject) => {
          sleep(1000);
          resolve("string1");
        });
        const p4 = new Promise((resolve, reject) => {
          sleep(200);
          resolve("string2");
        });

        return Promise.all([p1, p2, p3, p4]).then((values) => {
          return values;
        });
      }

      async function Handler() {
          const result = await multiplePromises();
          return result.join(" ");
      }
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, StrEq(R"("some cool string1 string2")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, BatchExecute) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  int res_count = 0;
  constexpr size_t kBatchSize = 5;
  absl::Notification load_finished;
  absl::Notification execute_finished;
  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = InvocationStrRequest<>();
    execution_obj.id = "foo";
    execution_obj.version_string = "v1";
    execution_obj.handler_name = "Handler";
    execution_obj.input.push_back(R"("Foobar")");

    std::vector<InvocationStrRequest<>> batch(kBatchSize, execution_obj);
    status = roma_service->BatchExecute(
        batch,
        [&](const std::vector<absl::StatusOr<ResponseObject>>& batch_resp) {
          for (auto resp : batch_resp) {
            EXPECT_TRUE(resp.ok());
            EXPECT_THAT(resp->resp, StrEq(R"("Hello world! \"Foobar\"")"));
          }
          res_count = batch_resp.size();
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  load_finished.WaitForNotification();
  execute_finished.WaitForNotification();
  EXPECT_EQ(res_count, kBatchSize);

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest,
     BatchExecuteShouldExecuteAllRequestsEvenWithSmallQueues) {
  Config config;
  // Queue of size one and 10 workers. Incoming work should block while
  // workers are busy and can't pick up items.
  config.worker_queue_max_items = 1;
  config.number_of_workers = 10;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  absl::Mutex mu;
  int res_count = 0;
  // Large batch
  constexpr size_t kBatchSize = 100;
  absl::Notification load_finished;
  absl::Notification execute_finished;
  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = InvocationStrRequest<>();
    execution_obj.id = "foo";
    execution_obj.version_string = "v1";
    execution_obj.handler_name = "Handler";
    execution_obj.input.push_back(R"("Foobar")");

    std::vector<InvocationStrRequest<>> batch(kBatchSize, execution_obj);

    status = absl::Status(absl::StatusCode::kInternal, "fail");
    while (!status.ok()) {
      status = roma_service->BatchExecute(
          batch,
          [&](const std::vector<absl::StatusOr<ResponseObject>>& batch_resp) {
            for (auto resp : batch_resp) {
              EXPECT_TRUE(resp.ok());
              EXPECT_THAT(resp->resp, StrEq(R"("Hello world! \"Foobar\"")"));
            }
            res_count = batch_resp.size();
            execute_finished.Notify();
          });
    }
    EXPECT_TRUE(status.ok());
  }

  load_finished.WaitForNotification();
  execute_finished.WaitForNotification();
  EXPECT_EQ(res_count, kBatchSize);

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, MultiThreadedBatchExecuteSmallQueue) {
  Config config;
  config.worker_queue_max_items = 1;
  config.number_of_workers = 10;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());
  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    ASSERT_TRUE(status.ok());
    load_finished.WaitForNotification();
  }

  absl::Mutex res_count_mu;
  int res_count ABSL_GUARDED_BY(res_count_mu) = 0;

  constexpr int kNumThreads = 10;
  constexpr size_t kBatchSize = 100;
  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; i++) {
    threads.emplace_back([&, i]() {
      absl::Notification local_execute;
      InvocationStrRequest<> execution_obj{};
      execution_obj.id = "foo";
      execution_obj.version_string = "v1";
      execution_obj.handler_name = "Handler";
      execution_obj.input.push_back(absl::StrCat(R"(")", "Foobar", i, R"(")"));

      std::vector<InvocationStrRequest<>> batch(kBatchSize, execution_obj);

      auto batch_callback =
          [&,
           i](const std::vector<absl::StatusOr<ResponseObject>>& batch_resp) {
            for (auto resp : batch_resp) {
              if (resp.ok()) {
                EXPECT_THAT(resp->resp,
                            StrEq(absl::StrCat("\"Hello world! \\\"Foobar", i,
                                               "\\\"\"")));
              } else {
                ADD_FAILURE() << "resp is NOT OK.";
              }
            }
            {
              absl::MutexLock l(&res_count_mu);
              res_count += batch_resp.size();
            }
            local_execute.Notify();
          };
      while (!roma_service->BatchExecute(batch, batch_callback).ok()) {}

      // Thread cannot join until batch_callback is called.
      local_execute.WaitForNotification();
    });
  }

  for (auto& t : threads) {
    t.join();
  }
  {
    absl::MutexLock l(&res_count_mu);
    EXPECT_EQ(res_count, kBatchSize * kNumThreads);
  }

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ExecuteCodeConcurrently) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  absl::Notification load_finished;
  size_t total_runs = 10;
  std::vector<std::string> results(total_runs);
  std::vector<absl::Notification> finished(total_runs);
  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    for (auto i = 0u; i < total_runs; ++i) {
      auto code_obj = std::make_unique<InvocationSharedRequest<>>();
      code_obj->id = "foo";
      code_obj->version_string = "v1";
      code_obj->handler_name = "Handler";
      code_obj->input.push_back(std::make_shared<std::string>(
          R"("Foobar)" + std::to_string(i) + R"(")"));

      status = roma_service->Execute(
          std::move(code_obj),
          [&, i](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
            EXPECT_TRUE(resp->ok());
            if (resp->ok()) {
              auto& code_resp = **resp;
              results[i] = code_resp.resp;
            }
            finished[i].Notify();
          });
      EXPECT_TRUE(status.ok());
    }
  }

  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  for (auto i = 0u; i < total_runs; ++i) {
    finished[i].WaitForNotificationWithTimeout(absl::Seconds(30));
    std::string expected_result = std::string(R"("Hello world! )") +
                                  std::string("\\\"Foobar") +
                                  std::to_string(i) + std::string("\\\"\"");
    EXPECT_THAT(results[i], StrEq(expected_result));
  }

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

void StringInStringOutFunction(FunctionBindingPayload<>& wrapper) {
  wrapper.io_proto.set_output_string(wrapper.io_proto.input_string() +
                                     " String from C++");
}

TEST(SandboxedServiceTest,
     CanRegisterBindingAndExecuteCodeThatCallsItWithInputAndOutputString) {
  Config config;
  config.number_of_workers = 2;
  auto function_binding_object = std::make_unique<FunctionBindingObjectV2<>>();
  function_binding_object->function = StringInStringOutFunction;
  function_binding_object->function_name = "cool_function";
  config.RegisterFunctionBinding(std::move(function_binding_object));

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return cool_function(input);}
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("Foobar")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, StrEq(R"("Foobar String from C++")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

void ListOfStringInListOfStringOutFunction(FunctionBindingPayload<>& wrapper) {
  int i = 1;

  for (auto& str : wrapper.io_proto.input_list_of_string().data()) {
    wrapper.io_proto.mutable_output_list_of_string()->mutable_data()->Add(
        str + " Some other stuff " + std::to_string(i++));
  }
}

TEST(
    SandboxedServiceTest,
    CanRegisterBindingAndExecuteCodeThatCallsItWithInputAndOutputListOfString) {
  Config config;
  config.number_of_workers = 2;
  auto function_binding_object = std::make_unique<FunctionBindingObjectV2<>>();
  function_binding_object->function = ListOfStringInListOfStringOutFunction;
  function_binding_object->function_name = "cool_function";
  config.RegisterFunctionBinding(std::move(function_binding_object));

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler() { some_array = ["str 1", "str 2", "str 3"]; return cool_function(some_array);}
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(
      result,
      StrEq(
          R"(["str 1 Some other stuff 1","str 2 Some other stuff 2","str 3 Some other stuff 3"])"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

void MapOfStringInMapOfStringOutFunction(FunctionBindingPayload<>& wrapper) {
  for (auto& [key, value] : wrapper.io_proto.input_map_of_string().data()) {
    std::string new_key;
    std::string new_val;
    if (key == "key-a") {
      new_key = key + std::to_string(1);
      new_val = value + std::to_string(1);
    } else {
      new_key = key + std::to_string(2);
      new_val = value + std::to_string(2);
    }
    (*wrapper.io_proto.mutable_output_map_of_string()
          ->mutable_data())[new_key] = new_val;
  }
}

TEST(SandboxedServiceTest,
     CanRegisterBindingAndExecuteCodeThatCallsItWithInputAndOutputMapOfString) {
  Config config;
  config.number_of_workers = 2;
  auto function_binding_object = std::make_unique<FunctionBindingObjectV2<>>();
  function_binding_object->function = MapOfStringInMapOfStringOutFunction;
  function_binding_object->function_name = "cool_function";
  config.RegisterFunctionBinding(std::move(function_binding_object));

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler() {
      some_map = [["key-a","value-a"], ["key-b","value-b"]];
      // Since we can't stringify a Map, we build an array from the resulting map entries.
      returned_map = cool_function(new Map(some_map));
      return Array.from(returned_map.entries());
    }
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  // Since the map makes it over the wire, we can't guarantee the order of the
  // keys so we assert that the expected key-value pairs are present.
  EXPECT_THAT(result, HasSubstr(R"(["key-a1","value-a1"])"));
  EXPECT_THAT(result, HasSubstr(R"(["key-b2","value-b2"])"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

void StringInStringOutFunctionWithNoInputParams(
    FunctionBindingPayload<>& wrapper) {
  // Params are all empty
  EXPECT_FALSE(wrapper.io_proto.has_input_string());
  EXPECT_FALSE(wrapper.io_proto.has_input_list_of_string());
  EXPECT_FALSE(wrapper.io_proto.has_input_map_of_string());

  wrapper.io_proto.set_output_string("String from C++");
}

TEST(SandboxedServiceTest, CanCallFunctionBindingThatDoesNotTakeAnyArguments) {
  Config config;
  config.number_of_workers = 2;
  auto function_binding_object = std::make_unique<FunctionBindingObjectV2<>>();
  function_binding_object->function =
      StringInStringOutFunctionWithNoInputParams;
  function_binding_object->function_name = "cool_function";
  config.RegisterFunctionBinding(std::move(function_binding_object));

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler() { return cool_function();}
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  EXPECT_THAT(result, StrEq(R"("String from C++")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, CanExecuteWasmCode) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./scp/cc/roma/testing/"
      "cpp_wasm_string_in_string_out_example/"
      "string_in_string_out.wasm");
  auto wasm_code =
      std::string(reinterpret_cast<char*>(wasm_bin.data()), wasm_bin.size());
  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->wasm = wasm_code;

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("Foobar")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, StrEq(R"("Foobar Hello World from WASM")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldReturnCorrectErrorForDifferentException) {
  Config config;
  config.number_of_workers = 1;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_timeout;
  absl::Notification execute_failed;
  absl::Notification execute_success;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"""(
    function sleep(milliseconds) {
      const date = Date.now();
      let currentDate = null;
      do {
        currentDate = Date.now();
      } while (currentDate - date < milliseconds);
    }
    let x;
    function hello_js(input) {
        sleep(200);
        if (input === undefined) {
          return x.value;
        }
        return "Hello world!"
      }
    )""";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  // The execution should timeout as the kTimeoutDurationTag value is too small.
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "hello_js";
    execution_obj->tags[kTimeoutDurationTag] = "100ms";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_FALSE(resp->ok());
          // Timeout error.
          EXPECT_THAT(resp->status().message(),
                      StrEq("V8 execution terminated due to timeout."));
          execute_timeout.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  // The execution should return invoking error as it try to get value from
  // undefined var.
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "hello_js";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_FALSE(resp->ok());
          EXPECT_THAT(resp->status().message(),
                      StrEq("Error when invoking the handler."));
          execute_failed.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  // The execution should success.
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "hello_js";
    execution_obj->input.push_back(R"("0")");
    execution_obj->tags[kTimeoutDurationTag] = "300ms";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          ASSERT_TRUE(resp->ok());
          auto& code_resp = **resp;
          EXPECT_THAT(code_resp.resp, StrEq(R"("Hello world!")"));
          execute_success.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_timeout.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_failed.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_success.WaitForNotificationWithTimeout(absl::Seconds(10));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

void EchoFunction(FunctionBindingPayload<>& wrapper) {
  wrapper.io_proto.set_output_string(wrapper.io_proto.input_string());
}

TEST(SandboxedServiceTest,
     ShouldRespectJsHeapLimitsAndContinueWorkingAfterWorkerRestart) {
  Config config;
  // Only one worker so we can make sure it's actually restarted.
  config.number_of_workers = 1;
  // Too large an allocation will cause the worker to crash and be restarted
  // since we're giving it a max of 15 MB of heap for JS execution.
  config.ConfigureJsEngineResourceConstraints(1 /*initial_heap_size_in_mb*/,
                                              15 /*maximum_heap_size_in_mb*/);
  // We register a hook to make sure it continues to work when the worker is
  // restarted
  auto function_binding_object = std::make_unique<FunctionBindingObjectV2<>>();
  function_binding_object->function = EchoFunction;
  function_binding_object->function_name = "echo_function";
  config.RegisterFunctionBinding(std::move(function_binding_object));
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    // Dummy code to allocate memory based on input
    code_obj->js = R"(
        function Handler(input) {
          const bigObject = [];
          for (let i = 0; i < 1024*512*Number(input); i++) {
            var person = {
            name: 'test',
            age: 24,
            };
            bigObject.push(person);
          }
          return 233;
        }
      )";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }

  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo2";
    code_obj->version_string = "v2";
    // Dummy code to exercise binding
    code_obj->js = R"(
        function Handler(input) {
          return echo_function(input);
        }
      )";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }

  {
    absl::Notification execute_finished;
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    // Large input which should fail
    execution_obj->input.push_back(R"("10")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_FALSE(resp->ok());
          EXPECT_THAT(
              resp->status().message(),
              StrEq("Sandbox worker crashed during execution of request."));
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }

  {
    absl::Notification execute_finished;
    std::string result;

    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    // Small input which should work
    execution_obj->input.push_back(R"("1")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());

    execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

    EXPECT_THAT(result, StrEq("233"));
  }

  {
    absl::Notification execute_finished;
    std::string result;

    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v2";
    execution_obj->handler_name = "Handler";
    // Small input which should work
    execution_obj->input.push_back(R"("Hello, World!")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());

    execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

    EXPECT_THAT(result, StrEq(R"("Hello, World!")"));
  }

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest,
     LoadingWasmModuleShouldFailIfMemoryRequirementIsNotMet) {
  {
    Config config;
    // This module was compiled with a memory requirement of 10MiB (160 pages
    // - each page is 64KiB). When we set the limit to 150 pages, it fails to
    // properly build the WASM object.
    config.max_wasm_memory_number_of_pages = 150;
    config.number_of_workers = 1;

    auto roma_service = std::make_unique<RomaService<>>(config);
    auto status = roma_service->Init();
    EXPECT_TRUE(status.ok());

    auto wasm_bin = WasmTestingUtils::LoadWasmFile(
        "./scp/cc/roma/testing/"
        "cpp_wasm_allocate_memory/allocate_memory.wasm");

    absl::Notification load_finished;
    {
      auto code_obj = std::make_unique<CodeObject>();
      code_obj->id = "foo";
      code_obj->version_string = "v1";
      code_obj->js = "";
      code_obj->wasm.assign(reinterpret_cast<char*>(wasm_bin.data()),
                            wasm_bin.size());

      status = roma_service->LoadCodeObj(
          std::move(code_obj),
          [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
            // Fails
            EXPECT_FALSE(resp->ok());
            EXPECT_THAT(resp->status().message(),
                        StrEq("Failed to create wasm object."));
            load_finished.Notify();
          });
      EXPECT_TRUE(status.ok());
    }

    load_finished.WaitForNotification();

    status = roma_service->Stop();
    EXPECT_TRUE(status.ok());
  }

  // We now load the same WASM but with the amount of memory it requires, and
  // it should work. Note that this requires restarting the service since this
  // limit is an initialization limit for the JS engine.

  {
    Config config;
    // This module was compiled with a memory requirement of 10MiB (160 pages
    // - each page is 64KiB). When we set the limit to 160 pages, it should be
    // able to properly build the WASM object.
    config.max_wasm_memory_number_of_pages = 160;
    config.number_of_workers = 1;

    auto roma_service = std::make_unique<RomaService<>>(config);
    auto status = roma_service->Init();
    EXPECT_TRUE(status.ok());

    auto wasm_bin = WasmTestingUtils::LoadWasmFile(
        "./scp/cc/roma/testing/"
        "cpp_wasm_allocate_memory/allocate_memory.wasm");

    absl::Notification load_finished;
    {
      auto code_obj = std::make_unique<CodeObject>();
      code_obj->id = "foo";
      code_obj->version_string = "v1";
      code_obj->js = "";
      code_obj->wasm.assign(reinterpret_cast<char*>(wasm_bin.data()),
                            wasm_bin.size());

      status = roma_service->LoadCodeObj(
          std::move(code_obj),
          [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
            // Loading works
            EXPECT_TRUE(resp->ok());
            load_finished.Notify();
          });
      EXPECT_TRUE(status.ok());
    }

    load_finished.WaitForNotification();

    status = roma_service->Stop();
    EXPECT_TRUE(status.ok());
  }
}

TEST(SandboxedServiceTest, ShouldGetMetricsInResponse) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("Foobar")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }

          EXPECT_GT(
              resp->value().metrics["roma.metric.sandboxed_code_run_duration"],
              absl::Duration());
          EXPECT_GT(resp->value().metrics["roma.metric.code_run_duration"],
                    absl::Duration());
          EXPECT_GT(
              resp->value().metrics["roma.metric.json_input_parsing_duration"],
              absl::Duration());
          EXPECT_GT(resp->value()
                        .metrics["roma.metric.js_engine_handler_call_duration"],
                    absl::Duration());
          std::cout << "Metrics:" << std::endl;
          for (const auto& pair : resp->value().metrics) {
            std::cout << pair.first << ": " << pair.second << std::endl;
          }

          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldRespectCodeObjectCacheSize) {
  Config config;
  config.number_of_workers = 2;
  // Only one version
  config.code_version_cache_size = 1;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;

  {
    // Load version 1
    absl::Notification load_finished;
    {
      auto code_obj = std::make_unique<CodeObject>();
      code_obj->id = "foo";
      code_obj->version_string = "v1";
      code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world1! " + JSON.stringify(input);
    }
  )JS_CODE";

      status = roma_service->LoadCodeObj(
          std::move(code_obj),
          [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
            EXPECT_TRUE(resp->ok());
            load_finished.Notify();
          });
      EXPECT_TRUE(status.ok());
    }

    // Execute version 1
    {
      absl::Notification execute_finished;
      auto execution_obj = std::make_unique<InvocationStrRequest<>>();
      execution_obj->id = "foo";
      execution_obj->version_string = "v1";
      execution_obj->handler_name = "Handler";
      execution_obj->input.push_back(R"("Foobar")");

      status = roma_service->Execute(
          std::move(execution_obj),
          [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
            EXPECT_TRUE(resp->ok());
            if (resp->ok()) {
              auto& code_resp = **resp;
              result = code_resp.resp;
            }
            execute_finished.Notify();
          });
      EXPECT_TRUE(status.ok());
      load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
    }
  }
  EXPECT_THAT(result, StrEq(R"("Hello world1! \"Foobar\"")"));

  // Load version 2
  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v2";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world2! " + JSON.stringify(input);
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }

  // Execute version 1 - Should fail since the cache has one spot, and we
  // loaded a new version.
  {
    absl::Notification execute_finished;
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("Foobar")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          // Should fail
          EXPECT_FALSE(resp->ok());
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }

  result = "";

  // Execute version 2
  {
    absl::Notification execute_finished;
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v2";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("Foobar")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }
  EXPECT_THAT(result, StrEq(R"("Hello world2! \"Foobar\"")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldAllowLoadingVersionWhileDispatching) {
  Config config;
  config.number_of_workers = 2;
  // Up to 2 code versions at a time.
  config.code_version_cache_size = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;

  // Load version 1
  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world1! " + JSON.stringify(input);
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }

  // Start a batch execution
  {
    absl::Notification execute_finished;
    {
      std::vector<InvocationStrRequest<>> batch;
      for (int i = 0; i < 50; i++) {
        InvocationStrRequest<> req;
        req.id = "foo";
        req.version_string = "v1";
        req.handler_name = "Handler";
        req.input.push_back(R"("Foobar")");
        batch.push_back(req);
      }
      auto batch_result = roma_service->BatchExecute(
          batch,
          [&](const std::vector<absl::StatusOr<ResponseObject>>& batch_resp) {
            for (auto& resp : batch_resp) {
              EXPECT_TRUE(resp.ok());
              if (resp.ok()) {
                auto& code_resp = resp.value();
                result = code_resp.resp;
              }
            }
            execute_finished.Notify();
          });
    }

    // Load version 2 while execution is happening
    absl::Notification load_finished;
    {
      auto code_obj = std::make_unique<CodeObject>();
      code_obj->id = "foo";
      code_obj->version_string = "v2";
      code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world2! " + JSON.stringify(input);
    }
  )JS_CODE";

      status = roma_service->LoadCodeObj(
          std::move(code_obj),
          [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
            EXPECT_TRUE(resp->ok());
            load_finished.Notify();
          });
      EXPECT_TRUE(status.ok());
    }
    load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
    execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }

  EXPECT_THAT(result, StrEq(R"("Hello world1! \"Foobar\"")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldTimeOutIfExecutionExceedsDeadline) {
  Config config;
  config.number_of_workers = 1;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;

  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    // Code to sleep for the number of milliseconds passed as input
    code_obj->js = R"JS_CODE(
    function sleep(milliseconds) {
      const date = Date.now();
      let currentDate = null;
      do {
        currentDate = Date.now();
      } while (currentDate - date < milliseconds);
    }

    function Handler(input) {
      sleep(parseInt(input));
      return "Hello world!";
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }

  privacy_sandbox::server_common::Stopwatch timer;

  {
    absl::Notification execute_finished;
    // Should not timeout since we only sleep for 9 sec but the timeout is 10
    // sec.
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("9000")");
    execution_obj->tags[kTimeoutDurationTag] = "10000ms";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    execute_finished.WaitForNotificationWithTimeout(absl::Seconds(30));
  }

  auto elapsed_time_ms = absl::ToDoubleMilliseconds(timer.GetElapsedTime());
  // Should have elapsed more than 9sec.
  EXPECT_GE(elapsed_time_ms, 9000);
  // But less than 10.
  EXPECT_LE(elapsed_time_ms, 10000);
  EXPECT_THAT(result, StrEq(R"("Hello world!")"));

  result = "";
  timer.Reset();

  {
    absl::Notification execute_finished;
    // Should time out since we sleep for 11 which is longer than the 10
    // sec timeout.
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("11000")");
    execution_obj->tags[kTimeoutDurationTag] = "10000ms";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_FALSE(resp->ok());
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    execute_finished.WaitForNotificationWithTimeout(absl::Seconds(30));
  }

  elapsed_time_ms = absl::ToDoubleMilliseconds(timer.GetElapsedTime());
  // Should have elapsed more than 10sec since that's our
  // timeout.
  EXPECT_GE(elapsed_time_ms, 10000);
  // But less than 11
  EXPECT_LE(elapsed_time_ms, 11000);
  EXPECT_THAT(result, IsEmpty());

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldGetCompileErrorForBadJsCode) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  absl::Notification load_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    // Bad JS code.
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_FALSE(resp->ok());
          EXPECT_THAT(resp->status().message(),
                      StrEq("Failed to compile JavaScript code object."));
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldGetExecutionErrorWhenJsCodeThrowError) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  absl::Notification load_finished;
  absl::Notification execute_finished;
  absl::Notification execute_failed;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
      function Handler(input) {
        if (input === "0") {
          throw new Error('Yeah...Input cannot be 0!');
        }
        return "Hello world! " + JSON.stringify(input);
      }
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back("9000");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          ASSERT_TRUE(resp->ok());
          auto& code_resp = **resp;
          EXPECT_THAT(code_resp.resp, StrEq(R"("Hello world! 9000")"));
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("0")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          ASSERT_FALSE(resp->ok());
          EXPECT_THAT(resp->status().message(),
                      StrEq("Error when invoking the handler."));
          execute_failed.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_failed.WaitForNotificationWithTimeout(absl::Seconds(10));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldGetExecutionErrorWhenJsCodeReturnUndefined) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  absl::Notification load_finished;
  absl::Notification execute_finished;
  absl::Notification execute_failed;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
      let x;
      function Handler(input) {
        if (input === "0") {
          return "Hello world! " + x.value;
        }
        return "Hello world! " + JSON.stringify(input);
      }
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back("9000");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          ASSERT_TRUE(resp->ok());
          auto& code_resp = **resp;
          EXPECT_THAT(code_resp.resp, StrEq(R"("Hello world! 9000")"));
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("0")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          ASSERT_FALSE(resp->ok());
          EXPECT_THAT(resp->status().message(),
                      StrEq("Error when invoking the handler."));
          execute_failed.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_failed.WaitForNotificationWithTimeout(absl::Seconds(10));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, CanHandleMultipleInputs) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(arg1, arg2) {
      return arg1 + arg2;
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("Foobar1")");
    execution_obj->input.push_back(R"(" Barfoo2")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, StrEq(R"("Foobar1 Barfoo2")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ErrorShouldBeExplicitWhenInputCannotBeParsed) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) {
      return input;
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    // Not a JSON string
    execution_obj->input.push_back("Foobar1");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_FALSE(resp->ok());
          // Should return failure
          EXPECT_THAT(resp->status().message(),
                      StrEq("Error parsing input as valid JSON."));
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest,
     ShouldGetErrorIfLoadFailsButExecutionIsSentForVersion) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;

  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    // Bad syntax so load should fail
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "123
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          // Load should have failed
          EXPECT_FALSE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }

  {
    absl::Notification execute_finished;
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("Foobar")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          // Execution should fail since load didn't work for this
          // code version
          EXPECT_FALSE(resp->ok());
          EXPECT_THAT(resp->status().message(),
                      StrEq("Could not find a stored context "
                            "for the execution request."));
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }

  // Should be able to load same version
  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler() { return "Hello there";}
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }

  // Execution should work now
  {
    absl::Notification execute_finished;
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          EXPECT_THAT((*resp)->resp, StrEq(R"("Hello there")"));
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

void ByteOutFunction(FunctionBindingPayload<>& wrapper) {
  const std::vector<uint8_t> data = {1, 2, 3, 4, 4, 3, 2, 1};
  wrapper.io_proto.set_output_bytes(data.data(), data.size());
}

TEST(SandboxedServiceTest,
     CanRegisterBindingAndExecuteCodeThatCallsItWithOutputBytes) {
  Config config;
  config.number_of_workers = 2;
  auto function_binding_object = std::make_unique<FunctionBindingObjectV2<>>();
  function_binding_object->function = ByteOutFunction;
  function_binding_object->function_name = "get_some_bytes";
  config.RegisterFunctionBinding(std::move(function_binding_object));

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler() {
      bytes = get_some_bytes();
      if (bytes instanceof Uint8Array) {
        return bytes;
      }

      return "Didn't work :(";
    }
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          auto& code_resp = **resp;
          result = code_resp.resp;
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  EXPECT_THAT(result,
              StrEq(R"({"0":1,"1":2,"2":3,"3":4,"4":4,"5":3,"6":2,"7":1})"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldBeAbleToOverwriteVersion) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;

  // Load v1
  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "version 1"; }
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }

  // Execute version 1
  {
    absl::Notification execute_finished;
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("Foobar")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          EXPECT_THAT((*resp)->resp, StrEq(R"("version 1")"));
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }

  // Should be able to load same version
  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler() { return "version 1 but updated";}
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }

  // Execution should run the new version of the code
  {
    absl::Notification execute_finished;
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          EXPECT_THAT((*resp)->resp, StrEq(R"("version 1 but updated")"));
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

void ByteInFunction(FunctionBindingPayload<>& wrapper) {
  auto data_len = wrapper.io_proto.input_bytes().size();
  EXPECT_EQ(5, data_len);
  auto byte_data = wrapper.io_proto.input_bytes().data();
  EXPECT_EQ(5, byte_data[0]);
  EXPECT_EQ(4, byte_data[1]);
  EXPECT_EQ(3, byte_data[2]);
  EXPECT_EQ(2, byte_data[3]);
  EXPECT_EQ(1, byte_data[4]);

  wrapper.io_proto.set_output_string("Hello there :)");
}

TEST(SandboxedServiceTest,
     CanRegisterBindingAndExecuteCodeThatCallsItWithInputBytes) {
  Config config;
  config.number_of_workers = 2;
  auto function_binding_object = std::make_unique<FunctionBindingObjectV2<>>();
  function_binding_object->function = ByteInFunction;
  function_binding_object->function_name = "set_some_bytes";
  config.RegisterFunctionBinding(std::move(function_binding_object));

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler() {
      bytes =  new Uint8Array(5);
      bytes[0] = 5;
      bytes[1] = 4;
      bytes[2] = 3;
      bytes[3] = 2;
      bytes[4] = 1;

      return set_some_bytes(bytes);
    }
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          auto& code_resp = **resp;
          result = code_resp.resp;
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  EXPECT_THAT(result, StrEq(R"str("Hello there :)")str"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, CanExecuteJSWithWasmCode) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    absl::flat_hash_map<std::string, std::string> tags;

    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
          const module = new WebAssembly.Module(addModule);
          const instance = new WebAssembly.Instance(module);
          function hello_js(a, b) {
            return instance.exports.add(a, b);
          }
    )JS_CODE";
    tags[kWasmCodeArrayName] = "addModule";

    code_obj->wasm_bin = kWasmBin;
    code_obj->tags = tags;

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "hello_js";
    execution_obj->input.push_back("1");
    execution_obj->input.push_back("2");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, StrEq("3"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, LoadJSWithWasmCodeShouldFailOnInvalidRequest) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  absl::Notification load_finished1;
  absl::Notification load_finished2;

  absl::flat_hash_map<std::string, std::string> tags;
  tags[kWasmCodeArrayName] = "addModule";
  std::string js_code = R"JS_CODE(
          const module = new WebAssembly.Module(addModule);
          const instance = new WebAssembly.Instance(module);
          function hello_js(a, b) {
            return instance.exports.add(a, b);
          }
  )JS_CODE";
  // Passing both the wasm and wasm_bin
  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = js_code;

    code_obj->wasm_bin = kWasmBin;
    code_obj->tags = tags;
    code_obj->wasm = "test";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {});
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.code(), absl::StatusCode::kInternal);
    EXPECT_THAT(status.message(),
                StrEq("Roma LoadCodeObj failed due to wasm code and wasm code "
                      "array conflict."));
  }

  // Missing JS code
  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->wasm_bin = kWasmBin;
    code_obj->tags = tags;

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {});
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.code(), absl::StatusCode::kInternal);
    EXPECT_THAT(status.message(),
                StrEq("Roma LoadCodeObj failed due to empty code content."));
  }

  // Missing wasm code array name tag
  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->js = js_code;
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->wasm_bin = kWasmBin;

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {});
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.code(), absl::StatusCode::kInternal);
    EXPECT_THAT(status.message(),
                StrEq("Roma LoadCodeObj failed due to empty wasm_bin or "
                      "missing wasm code array name tag."));
  }

  // Missing wasm_bin
  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->js = js_code;
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->tags = tags;

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {});
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.code(), absl::StatusCode::kInternal);
    EXPECT_THAT(status.message(),
                StrEq("Roma LoadCodeObj failed due to empty wasm_bin or "
                      "missing wasm code array name tag."));
  }

  // Wrong wasm array name tag
  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->js = js_code;
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->wasm_bin = kWasmBin;
    tags[kWasmCodeArrayName] = "wrongName";
    code_obj->tags = tags;

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_FALSE(resp->ok());
          load_finished1.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  // Invalid wasm code array
  {
    std::vector<uint8_t> invalid_wasm_bin{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07,
        0x01, 0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a,
        0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b};
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->js = js_code;
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->wasm_bin = invalid_wasm_bin;
    tags[kWasmCodeArrayName] = "addModule";
    code_obj->tags = tags;

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_FALSE(resp->ok());
          load_finished2.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  load_finished1.WaitForNotificationWithTimeout(absl::Seconds(10));
  load_finished2.WaitForNotificationWithTimeout(absl::Seconds(10));
  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

// TODO(b/311435456): Reenable when cause of flakiness found.
TEST(SandboxedServiceTest, DISABLED_CanExecuteJSWithWasmCodeWithStandaloneJS) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
          function hello_js(a, b) {
            return a + b;
          }
  )JS_CODE";

    code_obj->wasm_bin = kWasmBin;
    absl::flat_hash_map<std::string, std::string> tags;
    tags[kWasmCodeArrayName] = "addModule";
    code_obj->tags = tags;

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "hello_js";
    execution_obj->input.push_back("1");
    execution_obj->input.push_back("2");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, StrEq("3"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest,
     ShouldBeAbleToExecuteJsWithWasmBinEvenAfterWorkerCrash) {
  Config config;
  // Only one worker so we can make sure it's actually restarted.
  config.number_of_workers = 1;
  // Too large an allocation will cause the worker to crash and be restarted
  // since we're giving it a max of 15 MB of heap for JS execution.
  config.ConfigureJsEngineResourceConstraints(1 /*initial_heap_size_in_mb*/,
                                              15 /*maximum_heap_size_in_mb*/);
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  absl::Notification load_finished;

  // dummy code that gets killed by the JS engine if it allocates too much
  // memory
  std::string js_code = R"JS_CODE(
          const module = new WebAssembly.Module(addModule);
          const instance = new WebAssembly.Instance(module);
          function Handler(a, b, c) {
            const bigObject = [];
            for (let i = 0; i < 1024*512*Number(c); i++) {
              var person = {
              name: 'test',
              age: 24,
              };
              bigObject.push(person);
            }
            return instance.exports.add(a, b);
          }
  )JS_CODE";
  absl::flat_hash_map<std::string, std::string> tags;
  tags[kWasmCodeArrayName] = "addModule";

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->version_string = "v1";
    code_obj->js = js_code;
    code_obj->id = "foo";
    code_obj->wasm_bin = kWasmBin;
    code_obj->tags = tags;

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  {
    absl::Notification execute_finished;
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    // Large input which should fail
    execution_obj->input.push_back("1");
    execution_obj->input.push_back("2");
    execution_obj->input.push_back("10");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_FALSE(resp->ok());
          EXPECT_THAT(
              resp->status().message(),
              StrEq("Sandbox worker crashed during execution of request."));
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  }

  {
    absl::Notification execute_finished;
    std::string result;

    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    // Small input which should work
    execution_obj->input.push_back("1");
    execution_obj->input.push_back("2");
    execution_obj->input.push_back("1");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());

    execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

    EXPECT_THAT(result, StrEq("3"));
  }

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, LoadingShouldSucceedIfPayloadLargerThanBufferSize) {
  Config config;
  config.number_of_workers = 2;
  // The buffer size is 1MB.
  config.sandbox_request_response_shared_buffer_size_mb = 1;
  config.enable_sandbox_sharing_request_response_with_buffer_only = false;

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification success_execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    // The js payload size is larger than the Buffer capacity.
    auto payload_size = 1024 * 1024 * 1.2;
    std::string dummy_js_string(payload_size, 'a');
    code_obj->js = "function Handler(input) { let x = \"" + dummy_js_string +
                   "\"; return \"Hello world! \"}";
    EXPECT_GE(code_obj->js.length(), payload_size);

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    ASSERT_TRUE(status.ok());
  }

  // execute success
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back("\"Foobar\"");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          ASSERT_TRUE(resp->ok());
          auto& code_resp = **resp;
          result = code_resp.resp;
          success_execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  success_execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, StrEq(R"("Hello world! ")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ExecutionShouldSucceedIfRequestPayloadOversize) {
  Config config;
  config.number_of_workers = 2;
  // The buffer size is 1MB.
  config.sandbox_request_response_shared_buffer_size_mb = 1;
  config.enable_sandbox_sharing_request_response_with_buffer_only = false;

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification oversize_execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    // The input payload size is larger than the Buffer capacity.
    auto payload_size = 1024 * 1024 * 1.2;
    std::string dummy_string(payload_size, 'A');
    execution_obj->input.push_back("\"" + dummy_string + "\"");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          ASSERT_TRUE(resp->ok());
          auto& code_resp = **resp;
          EXPECT_GE(code_resp.resp.length(), payload_size);
          oversize_execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  oversize_execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ExecutionShouldSucceedIfResponsePayloadOversize) {
  Config config;
  config.number_of_workers = 2;
  // The buffer size is 1MB.
  config.sandbox_request_response_shared_buffer_size_mb = 1;
  config.enable_sandbox_sharing_request_response_with_buffer_only = false;

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  absl::Notification load_finished;
  absl::Notification oversize_execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    // Will generate a response with input size.
    code_obj->js = R"JS_CODE(
    function Handler(input) {
      let dummy_string = 'x'.repeat(input);
      return "Hello world! " + JSON.stringify(dummy_string);
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  // execute success when the response payload size is larger than buffer
  // capacity.
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    // The response payload size is larger than the Buffer capacity.
    auto payload_size = 1024 * 1024 * 1.2;
    execution_obj->input.push_back("\"" + std::to_string(payload_size) + "\"");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          ASSERT_TRUE(resp->ok());
          auto& code_resp = **resp;
          EXPECT_GE(code_resp.resp.length(), payload_size);
          oversize_execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  oversize_execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest,
     EnableBufferOnlyLoadingShouldFailGracefullyIfPayloadOversize) {
  Config config;
  config.number_of_workers = 2;
  // The buffer size is 1MB.
  config.sandbox_request_response_shared_buffer_size_mb = 1;
  config.enable_sandbox_sharing_request_response_with_buffer_only = true;

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    // The js payload size is larger than the Buffer capacity.
    auto payload_size = 1024 * 1024 * 1.2;
    std::string dummy_js_string(payload_size, 'A');
    code_obj->js = "\"" + dummy_js_string + "\"";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_FALSE(resp->ok());
          EXPECT_THAT(resp->status().message(),
                      StrEq("The size of request serialized data is "
                            "larger than the Buffer capacity."));
          load_finished.Notify();
        });
    ASSERT_TRUE(status.ok());
  }

  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest,
     EnableBufferOnlyExecutionShouldFailGracefullyIfRequestPayloadOversize) {
  Config config;
  config.number_of_workers = 2;
  // The buffer size is 1MB.
  config.sandbox_request_response_shared_buffer_size_mb = 1;
  config.enable_sandbox_sharing_request_response_with_buffer_only = true;

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification success_execute_finished;
  absl::Notification failed_execute_finished;
  std::string retry_result;
  absl::Notification retry_success_execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  // execute success
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back("\"Foobar\"");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          ASSERT_TRUE(resp->ok());
          auto& code_resp = **resp;
          result = code_resp.resp;
          success_execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  // Failure in execution as oversize input.
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    // The input payload size is larger than the Buffer capacity.
    auto payload_size = 1024 * 1024 * 1.2;
    std::string dummy_string(payload_size, 'A');
    execution_obj->input.push_back("\"" + dummy_string + "\"");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_FALSE(resp->ok());
          EXPECT_THAT(resp->status().message(),
                      StrEq("The size of request serialized data is "
                            "larger than the Buffer capacity."));
          failed_execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  // execute success
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back("\"Foobar\"");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          ASSERT_TRUE(resp->ok());
          auto& code_resp = **resp;
          retry_result = code_resp.resp;
          retry_success_execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  success_execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  failed_execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  retry_success_execute_finished.WaitForNotificationWithTimeout(
      absl::Seconds(10));
  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));
  EXPECT_THAT(retry_result, StrEq(R"("Hello world! \"Foobar\"")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest,
     EnableBufferOnlyExecutionShouldFailGracefullyIfResponsePayloadOversize) {
  Config config;
  config.number_of_workers = 10;
  // The buffer size is 1MB.
  config.sandbox_request_response_shared_buffer_size_mb = 1;
  config.enable_sandbox_sharing_request_response_with_buffer_only = true;

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  absl::Notification load_finished;
  absl::Notification success_execute_finished;
  absl::Notification failed_execute_finished;
  absl::Notification retry_success_execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    // Will generate a response with input size.
    code_obj->js = R"JS_CODE(
    function Handler(input) {
      let dummy_string = 'x'.repeat(input);
      return "Hello world! " + JSON.stringify(dummy_string);
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  // execute success
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back("\"1024\"");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          success_execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  // execute failed as the response payload size is larger than buffer
  // capacity.
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    // The response payload size is larger than the Buffer capacity.
    auto payload_size = 1024 * 1024 * 1.2;
    execution_obj->input.push_back("\"" + std::to_string(payload_size) + "\"");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          // Failure in execution
          EXPECT_FALSE(resp->ok());
          EXPECT_THAT(resp->status().message(),
                      StrEq("The size of response serialized data is "
                            "larger than the Buffer capacity."));
          failed_execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  // execute success
  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    auto payload_size = 1024 * 800;
    execution_obj->input.push_back("\"" + std::to_string(payload_size) + "\"");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          retry_success_execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  success_execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  failed_execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  retry_success_execute_finished.WaitForNotificationWithTimeout(
      absl::Seconds(10));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

}  // namespace google::scp::roma::test
