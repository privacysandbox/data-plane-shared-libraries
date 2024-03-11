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
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/config/config.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"
#include "src/util/duration.h"

using google::scp::roma::FunctionBindingPayload;
using google::scp::roma::sandbox::roma_service::RomaService;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::StrEq;

namespace google::scp::roma::test {
namespace {
TEST(SandboxedServiceTest, InitStop) {
  Config config;
  config.number_of_workers = 2;

  RomaService roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());
  EXPECT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest,
     ShouldFailToInitializeIfVirtualMemoryCapIsTooLittle) {
  Config config;
  config.number_of_workers = 2;
  config.max_worker_virtual_memory_mb = 10;

  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.message(), StrEq("Receiving TLV value failed"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, StopGracefullyWithPendingLoads) {
  RomaService<> roma_service(Config{});
  ASSERT_TRUE(roma_service.Init().ok());
  for (int i = 0; i < 100; ++i) {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = absl::StrCat("foo", i),
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE",
    });
    ASSERT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [](absl::StatusOr<ResponseObject> resp) {
                                   EXPECT_TRUE(resp.ok());
                                 })
                    .ok());
  }
  EXPECT_TRUE(roma_service.Stop().ok());
}

TEST(SandboxedServiceTest, ExecuteCode) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
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
            .input = {R"("Foobar")"},
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
  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, CanRegisterGrpcServices) {
  Config config;
  config.number_of_workers = 2;
  config.enable_native_function_grpc_server = true;
  config.RegisterService(std::make_unique<grpc_server::AsyncMultiService>(),
                         grpc_server::TestMethod1Handler<DefaultMetadata>(),
                         grpc_server::TestMethod2Handler<DefaultMetadata>());

  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());
  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ExecuteCodeWithStringViewInput) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

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

    status = roma_service.LoadCodeObj(std::move(load_code_obj_request),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
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

    status = roma_service.Execute(std::move(execute_request),
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
  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldFailWithInvalidHandlerName) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;
  absl::Notification failed_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
      function Handler(input) {
        return "Hello world! " + JSON.stringify(input);
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
            .input = {R"("Foobar")"},
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

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "WrongHandler",
            .input = {R"("Foobar")"},
        });

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          EXPECT_EQ(resp.status().code(), absl::StatusCode::kInternal);
          failed_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      failed_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ExecuteCodeWithEmptyId) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
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
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("Foobar")"},
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
  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldAllowEmptyInputs) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(arg1, arg2) { return arg1; }
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
  EXPECT_THAT(result, StrEq("undefined"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldGetIdInResponse) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .id = "my_cool_id",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE",
    });

    status = roma_service.LoadCodeObj(
        std::move(code_obj), [&](absl::StatusOr<ResponseObject> resp) {
          EXPECT_TRUE(resp.ok());
          EXPECT_THAT(resp->id, StrEq("my_cool_id"));
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
            .input = {R"("Foobar")"},
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
  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest,
     ShouldReturnWithVersionNotFoundWhenExecutingAVersionThatHasNotBeenLoaded) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  // We don't load any code, just try to execute some version
  absl::Notification execute_finished;

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("Foobar")"},
        });

    status = roma_service.Execute(std::move(execution_obj),
                                  [&](absl::StatusOr<ResponseObject> resp) {
                                    // Execute should fail.
                                    EXPECT_FALSE(resp.ok());
                                    execute_finished.Notify();
                                  });
    EXPECT_TRUE(status.ok());
  }
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, CanRunAsyncJsCode) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
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
  EXPECT_THAT(result, StrEq(R"("some cool string1 string2")"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, BatchExecute) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  int res_count = 0;
  constexpr size_t kBatchSize = 5;
  absl::Notification load_finished;
  absl::Notification execute_finished;
  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
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
    auto execution_obj = InvocationStrRequest<>({
        .id = "foo",
        .version_string = "v1",
        .handler_name = "Handler",
        .input = {R"("Foobar")"},
    });

    std::vector<InvocationStrRequest<>> batch(kBatchSize, execution_obj);
    status = roma_service.BatchExecute(
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

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest,
     BatchExecuteShouldExecuteAllRequestsEvenWithSmallQueues) {
  Config config;
  // Queue of size one and 10 workers. Incoming work should block while
  // workers are busy and can't pick up items.
  config.worker_queue_max_items = 1;
  config.number_of_workers = 10;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  absl::Mutex mu;
  int res_count = 0;
  // Large batch
  constexpr size_t kBatchSize = 100;
  absl::Notification load_finished;
  absl::Notification execute_finished;
  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
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
    auto execution_obj = InvocationStrRequest<>({
        .id = "foo",
        .version_string = "v1",
        .handler_name = "Handler",
        .input = {R"("Foobar")"},
    });

    std::vector<InvocationStrRequest<>> batch(kBatchSize, execution_obj);

    status = absl::InternalError("fail");
    while (!status.ok()) {
      status = roma_service.BatchExecute(
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

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, MultiThreadedBatchExecuteSmallQueue) {
  Config config;
  config.worker_queue_max_items = 1;
  config.number_of_workers = 10;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());
  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
  )JS_CODE",
    });

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    ASSERT_TRUE(status.ok());
    load_finished.WaitForNotification();
  }

  absl::Mutex res_count_mu;
  int res_count = 0;

  constexpr int kNumThreads = 10;
  constexpr size_t kBatchSize = 100;
  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; i++) {
    threads.emplace_back([&, i]() {
      absl::Notification local_execute;
      InvocationStrRequest<> execution_obj{
          .id = "foo",
          .version_string = "v1",
          .handler_name = "Handler",
          .input = {absl::StrCat(R"(")", "Foobar", i, R"(")")},
      };

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
    absl::MutexLock l(&res_count_mu);
    EXPECT_EQ(res_count, kBatchSize * kNumThreads);
  }

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ExecuteCodeConcurrently) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  absl::Notification load_finished;
  size_t total_runs = 10;
  std::vector<std::string> results(total_runs);
  std::vector<absl::Notification> finished(total_runs);
  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
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
    for (auto i = 0u; i < total_runs; ++i) {
      auto code_obj =
          std::make_unique<InvocationSharedRequest<>>(InvocationSharedRequest<>{
              .id = "foo",
              .version_string = "v1",
              .handler_name = "Handler",
              .input = {std::make_shared<std::string>(
                  R"("Foobar)" + std::to_string(i) + R"(")")},
          });

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
    std::string expected_result =
        absl::StrCat(R"("Hello world! )", "\\\"Foobar", i, "\\\"\"");
    EXPECT_THAT(results[i], StrEq(expected_result));
  }

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldReturnCorrectErrorForDifferentException) {
  Config config;
  config.number_of_workers = 1;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_timeout;
  absl::Notification execute_failed;
  absl::Notification execute_success;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"""(
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
    )""",
    });

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
  }

  // The execution should timeout as the kTimeoutDurationTag value is too small.
  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "hello_js",
            .tags = {{std::string(kTimeoutDurationTag), "100ms"}},
        });

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          EXPECT_EQ(resp.status().code(), absl::StatusCode::kInternal);
          execute_timeout.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  // The execution should return invoking error as it try to get value from
  // undefined var.
  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "hello_js",
        });

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          EXPECT_EQ(resp.status().code(), absl::StatusCode::kInternal);
          execute_failed.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  // The execution should success.
  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "hello_js",
            .input = {R"("0")"},
            .tags = {{std::string(kTimeoutDurationTag), "300ms"}},
        });

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          ASSERT_TRUE(resp.ok());
          EXPECT_THAT(resp->resp, StrEq(R"("Hello world!")"));
          execute_success.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_timeout.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(execute_failed.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_success.WaitForNotificationWithTimeout(absl::Seconds(10)));

  status = roma_service.Stop();
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
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        // Dummy code to allocate memory based on input
        .js = R"(
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
      )",
    });

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  }

  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo2",
        .version_string = "v2",
        // Dummy code to exercise binding
        .js = R"(
        function Handler(input) {
          return echo_function(input);
        }
      )",
    });

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  }

  {
    absl::Notification execute_finished;
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            // Large input which should fail
            .input = {R"("10")"},
        });

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          EXPECT_FALSE(resp.ok());
          EXPECT_THAT(
              resp.status().message(),
              StrEq("Sandbox worker crashed during execution of request."));
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  }

  {
    absl::Notification execute_finished;
    std::string result;

    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            // Small input which should work
            .input = {R"("1")"},
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

    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));

    EXPECT_THAT(result, StrEq("233"));
  }

  {
    absl::Notification execute_finished;
    std::string result;

    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v2",
            .handler_name = "Handler",
            // Small input which should work
            .input = {R"("Hello, World!")"},
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

    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));

    EXPECT_THAT(result, StrEq(R"("Hello, World!")"));
  }

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldGetMetricsInResponse) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
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
            .input = {R"("Foobar")"},
        });

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          EXPECT_TRUE(resp.ok());
          if (resp.ok()) {
            result = std::move(resp->resp);
          }

          EXPECT_GT(resp->metrics["roma.metric.sandboxed_code_run_duration"],
                    absl::Duration());
          EXPECT_GT(resp->metrics["roma.metric.code_run_duration"],
                    absl::Duration());
          EXPECT_GT(resp->metrics["roma.metric.json_input_parsing_duration"],
                    absl::Duration());
          EXPECT_GT(
              resp->metrics["roma.metric.js_engine_handler_call_duration"],
              absl::Duration());
          std::cout << "Metrics:" << std::endl;
          for (const auto& pair : resp->metrics) {
            std::cout << pair.first << ": " << pair.second << std::endl;
          }

          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_THAT(result, StrEq(R"("Hello world! \"Foobar\"")"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldAllowLoadingVersionWhileDispatching) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;

  // Load version 1
  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) { return "Hello world1! " + JSON.stringify(input);
    }
  )JS_CODE",
    });

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  }

  // Start a batch execution
  {
    absl::Notification execute_finished;
    {
      std::vector<InvocationStrRequest<>> batch;
      for (int i = 0; i < 50; i++) {
        InvocationStrRequest<> req{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("Foobar")"},
        };
        batch.push_back(req);
      }
      auto batch_result = roma_service.BatchExecute(
          batch,
          [&](const std::vector<absl::StatusOr<ResponseObject>>& batch_resp) {
            for (auto& resp : batch_resp) {
              EXPECT_TRUE(resp.ok());
              if (resp.ok()) {
                result = std::move(resp->resp);
              }
            }
            execute_finished.Notify();
          });
    }

    // Load version 2 while execution is happening
    absl::Notification load_finished;
    {
      auto code_obj = std::make_unique<CodeObject>(CodeObject{
          .id = "foo",
          .version_string = "v2",
          .js = R"JS_CODE(
    function Handler(input) { return "Hello world2! " + JSON.stringify(input);
    }
  )JS_CODE",
      });

      status = roma_service.LoadCodeObj(
          std::move(code_obj), [&](absl::StatusOr<ResponseObject> resp) {
            EXPECT_TRUE(resp.ok());
            load_finished.Notify();
          });
      EXPECT_TRUE(status.ok());
    }
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  }

  EXPECT_THAT(result, StrEq(R"("Hello world1! \"Foobar\"")"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldTimeOutIfExecutionExceedsDeadline) {
  Config config;
  config.number_of_workers = 1;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;

  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        // Code to sleep for the number of milliseconds passed as input
        .js = R"JS_CODE(
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
  )JS_CODE",
    });

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  }

  privacy_sandbox::server_common::Stopwatch timer;

  {
    absl::Notification execute_finished;
    // Should not timeout since we only sleep for 9 sec but the timeout is 10
    // sec.
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("9000")"},
            .tags = {{std::string(kTimeoutDurationTag), "10000ms"}},
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
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(30)));
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
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("11000")"},
            .tags = {{std::string(kTimeoutDurationTag), "10000ms"}},
        });

    status = roma_service.Execute(std::move(execution_obj),
                                  [&](absl::StatusOr<ResponseObject> resp) {
                                    EXPECT_FALSE(resp.ok());
                                    execute_finished.Notify();
                                  });
    EXPECT_TRUE(status.ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(30)));
  }

  elapsed_time_ms = absl::ToDoubleMilliseconds(timer.GetElapsedTime());
  // Should have elapsed more than 10sec since that's our
  // timeout.
  EXPECT_GE(elapsed_time_ms, 10000);
  // But less than 11
  EXPECT_LE(elapsed_time_ms, 11000);
  EXPECT_THAT(result, IsEmpty());

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldGetCompileErrorForBadJsCode) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  absl::Notification load_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        // Bad JS code.
        .js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
  )JS_CODE",
    });

    status = roma_service.LoadCodeObj(
        std::move(code_obj), [&](absl::StatusOr<ResponseObject> resp) {
          EXPECT_EQ(resp.status().code(), absl::StatusCode::kInternal);
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldGetExecutionErrorWhenJsCodeThrowError) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  absl::Notification load_finished;
  absl::Notification execute_finished;
  absl::Notification execute_failed;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
      function Handler(input) {
        if (input === "0") {
          throw new Error('Yeah...Input cannot be 0!');
        }
        return "Hello world! " + JSON.stringify(input);
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
            .input = {"9000"},
        });

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          ASSERT_TRUE(resp.ok());
          EXPECT_THAT(resp->resp, StrEq(R"("Hello world! 9000")"));
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("0")"},
        });

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          ASSERT_EQ(resp.status().code(), absl::StatusCode::kInternal);
          execute_failed.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(execute_failed.WaitForNotificationWithTimeout(absl::Seconds(10)));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldGetExecutionErrorWhenJsCodeReturnUndefined) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  absl::Notification load_finished;
  absl::Notification execute_finished;
  absl::Notification execute_failed;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
      let x;
      function Handler(input) {
        if (input === "0") {
          return "Hello world! " + x.value;
        }
        return "Hello world! " + JSON.stringify(input);
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
            .input = {"9000"},
        });

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          ASSERT_TRUE(resp.ok());
          EXPECT_THAT(resp->resp, StrEq(R"("Hello world! 9000")"));
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("0")"},
        });

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          EXPECT_EQ(resp.status().code(), absl::StatusCode::kInternal);
          execute_failed.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(execute_failed.WaitForNotificationWithTimeout(absl::Seconds(10)));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, CanHandleMultipleInputs) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(arg1, arg2) {
      return arg1 + arg2;
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
            .input = {R"("Foobar1")", R"(" Barfoo2")"},
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
  EXPECT_THAT(result, StrEq(R"("Foobar1 Barfoo2")"));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ErrorShouldBeExplicitWhenInputCannotBeParsed) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) {
      return input;
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
            // Not a JSON string
            .input = {"Foobar1"},
        });

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          EXPECT_EQ(resp.status().code(), absl::StatusCode::kInternal);
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest,
     ShouldGetErrorIfLoadFailsButExecutionIsSentForVersion) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;

  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        // Bad syntax so load should fail
        .js = R"JS_CODE(
    function Handler(input) { return "123
    )JS_CODE",
    });

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        // Load should have failed
                                        EXPECT_FALSE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  }

  {
    absl::Notification execute_finished;
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("Foobar")"},
        });

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          // Execution should fail since load didn't work for this
          // code version
          EXPECT_EQ(resp.status().code(), absl::StatusCode::kInternal);
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  }

  // Should be able to load same version
  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler() { return "Hello there";}
    )JS_CODE",
    });

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  }

  // Execution should work now
  {
    absl::Notification execute_finished;
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
        });

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          EXPECT_TRUE(resp.ok());
          EXPECT_THAT(resp->resp, StrEq(R"("Hello there")"));
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  }

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}

TEST(SandboxedServiceTest, ShouldBeAbleToOverwriteVersion) {
  Config config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  auto status = roma_service.Init();
  ASSERT_TRUE(status.ok());

  std::string result;

  // Load v1
  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) { return "version 1"; }
    )JS_CODE",
    });

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  }

  // Execute version 1
  {
    absl::Notification execute_finished;
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("Foobar")"},
        });

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          EXPECT_TRUE(resp.ok());
          EXPECT_THAT(resp->resp, StrEq(R"("version 1")"));
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  }

  // Should be able to load same version
  {
    absl::Notification load_finished;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler() { return "version 1 but updated";}
    )JS_CODE",
    });

    status = roma_service.LoadCodeObj(std::move(code_obj),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        EXPECT_TRUE(resp.ok());
                                        load_finished.Notify();
                                      });
    EXPECT_TRUE(status.ok());
    ASSERT_TRUE(
        load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  }

  // Execution should run the new version of the code
  {
    absl::Notification execute_finished;
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
        });

    status = roma_service.Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          EXPECT_TRUE(resp.ok());
          EXPECT_THAT(resp->resp, StrEq(R"("version 1 but updated")"));
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  }

  status = roma_service.Stop();
  EXPECT_TRUE(status.ok());
}
}  // namespace
}  // namespace google::scp::roma::test
