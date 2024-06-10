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

#include "src/roma/sandbox/dispatcher/dispatcher.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"

using ::testing::StrEq;

namespace google::scp::roma::sandbox::dispatcher::test {
namespace {
std::vector<worker_api::WorkerSandboxApi> Workers(int num_workers) {
  std::vector<worker_api::WorkerSandboxApi> workers;
  workers.reserve(num_workers);
  for (int i = 0; i < num_workers; ++i) {
    workers.emplace_back(
        /*require_preload=*/true,
        /*native_js_function_comms_fd=*/-1,
        /*native_js_function_names=*/std::vector<std::string>(),
        /*rpc_method_names=*/std::vector<std::string>(),
        /*server_address*/ "",
        /*max_worker_virtual_memory_mb=*/0,
        /*js_engine_initial_heap_size_mb=*/0,
        /*js_engine_maximum_heap_size_mb=*/0,
        /*js_engine_max_wasm_memory_number_of_pages=*/0,
        /*sandbox_request_response_shared_buffer_size_mb=*/0,
        /*enable_sandbox_sharing_request_response_with_buffer_only=*/false);
    CHECK(workers.back().Init().ok());
    CHECK(workers.back().Run().ok());
  }
  return workers;
}
}  // namespace

TEST(DispatcherTest, CanRunCode) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK(worker.Stop().ok());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/10);

  CodeObject load_request{
      .id = "some_id",
      .version_string = "v1",
      .js = R"(function test(input) { return input + " Some string"; })",
  };
  absl::Notification done_loading;
  ASSERT_TRUE(dispatcher
                  .Load(std::move(load_request),
                        [&](absl::StatusOr<ResponseObject> resp) {
                          EXPECT_TRUE(resp.ok());
                          done_loading.Notify();
                        })
                  .ok());
  done_loading.WaitForNotification();

  InvocationStrRequest<> execute_request{
      .id = "some_id",
      .version_string = "v1",
      .handler_name = "test",
      .input = {R"("Hello")"},
  };
  absl::Notification done_executing;
  ASSERT_TRUE(dispatcher
                  .Invoke(std::move(execute_request),
                          [&](absl::StatusOr<ResponseObject> resp) {
                            EXPECT_TRUE(resp.ok());
                            EXPECT_THAT(resp->resp,
                                        StrEq(R"("Hello Some string")"));
                            done_executing.Notify();
                          })
                  .ok());
  done_executing.WaitForNotification();
}

TEST(DispatcherTest, CanRunCodeMultipleWorkers) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/3);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK(worker.Stop().ok());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/10);

  CodeObject load_request{
      .id = "some_id",
      .version_string = "v1",
      .js = R"(function test(input) { return input + " Some string"; })",
  };
  absl::Notification done_loading;
  ASSERT_TRUE(dispatcher
                  .Load(std::move(load_request),
                        [&](absl::StatusOr<ResponseObject> resp) {
                          EXPECT_TRUE(resp.ok());
                          done_loading.Notify();
                        })
                  .ok());
  done_loading.WaitForNotification();

  absl::BlockingCounter counter(10);
  for (int i = 0; i < 10; ++i) {
    InvocationStrRequest<> execute_request{
        .id = absl::StrCat("some_id_", i),
        .version_string = "v1",
        .handler_name = "test",
        .input = {R"("Hello")"},
    };
    ASSERT_TRUE(dispatcher
                    .Invoke(std::move(execute_request),
                            [&](absl::StatusOr<ResponseObject> resp) {
                              EXPECT_TRUE(resp.ok());
                              EXPECT_THAT(resp->resp,
                                          StrEq(R"("Hello Some string")"));
                              counter.DecrementCount();
                            })
                    .ok());
  }
  counter.Wait();
}

TEST(DispatcherTest, CanRunStringViewInputCode) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK(worker.Stop().ok());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/10);

  CodeObject load_request{
      .id = "some_id",
      .version_string = "v1",
      .js = "function test(input) { return input + \" Some string\"; }",
  };
  absl::Notification done_loading;
  ASSERT_TRUE(dispatcher
                  .Load(std::move(load_request),
                        [&](absl::StatusOr<ResponseObject> resp) {
                          EXPECT_TRUE(resp.ok());
                          done_loading.Notify();
                        })
                  .ok());
  done_loading.WaitForNotification();

  constexpr std::string_view kInputStrView{R"("Hello")"};
  InvocationStrViewRequest<> execute_request{
      .id = "some_id",
      .version_string = "v1",
      .handler_name = "test",
      .input = {kInputStrView},
  };
  absl::Notification done_executing;
  ASSERT_TRUE(dispatcher
                  .Invoke(std::move(execute_request),
                          [&](absl::StatusOr<ResponseObject> resp) {
                            EXPECT_TRUE(resp.ok());
                            EXPECT_THAT(resp->resp,
                                        StrEq(R"("Hello Some string")"));
                            done_executing.Notify();
                          })
                  .ok());
  done_executing.WaitForNotification();
}

TEST(DispatcherTest, CanHandleCodeFailures) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK(worker.Stop().ok());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/10);

  CodeObject load_bad_js_request{
      .id = "some_id",
      .version_string = "v1",
      // Bad JS
      .js = "function test(input) { ",
  };
  absl::Notification done_loading;
  ASSERT_TRUE(dispatcher
                  .Load(std::move(load_bad_js_request),
                        [&](absl::StatusOr<ResponseObject> resp) {
                          // That didn't work
                          EXPECT_FALSE(resp.ok());
                          done_loading.Notify();
                        })
                  .ok());
  done_loading.WaitForNotification();
}

TEST(DispatcherTest, CanHandleExecuteWithoutLoadFailure) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK(worker.Stop().ok());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/10);

  InvocationStrRequest<> execute_request{
      .id = "some_id",
      .version_string = "v1",
      .handler_name = "test",
      .input = {R"("Hello")"},
  };
  absl::Notification done_executing;
  ASSERT_TRUE(dispatcher
                  .Invoke(std::move(execute_request),
                          [&](absl::StatusOr<ResponseObject> resp) {
                            EXPECT_FALSE(resp.ok());
                            done_executing.Notify();
                          })
                  .ok());
  done_executing.WaitForNotification();
}

TEST(DispatcherTest, BroadcastShouldUpdateAllWorkers) {
  constexpr size_t kNumberOfWorkers = 5;
  std::vector<worker_api::WorkerSandboxApi> workers = Workers(kNumberOfWorkers);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK(worker.Stop().ok());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/100);

  CodeObject load_request{
      .id = "some_id",
      .version_string = "v1",
      .js = R"(test = (s) => s + " Some string";)",
  };
  absl::Notification done_loading;
  ASSERT_TRUE(dispatcher
                  .Load(std::move(load_request),
                        [&](absl::StatusOr<ResponseObject> resp) {
                          EXPECT_TRUE(resp.ok());
                          done_loading.Notify();
                        })
                  .ok());
  done_loading.WaitForNotification();

  absl::Mutex execution_count_mu;
  int execution_count = 0;

  // More than the number of workers to make sure the requests can indeed run in
  // all workers.
  constexpr int kRequestSent = kNumberOfWorkers * 3;
  for (int i = 0; i < kRequestSent; i++) {
    InvocationStrRequest<> execute_request{
        .id = absl::StrCat("some_id", i),
        .version_string = "v1",
        .handler_name = "test",
        .input = {absl::StrCat("\"Hello", i, "\"")},
    };
    ASSERT_TRUE(dispatcher
                    .Invoke(std::move(execute_request),
                            [&, i](absl::StatusOr<ResponseObject> resp) {
                              EXPECT_TRUE(resp.ok());
                              EXPECT_THAT(resp->resp,
                                          absl::StrCat(R"("Hello)", i,
                                                       R"( Some string")"));
                              absl::MutexLock l(&execution_count_mu);
                              execution_count++;
                            })
                    .ok());
  }

  {
    absl::MutexLock l(&execution_count_mu);
    auto condition_fn = [&] {
      execution_count_mu.AssertReaderHeld();
      return execution_count >= kRequestSent;
    };
    execution_count_mu.Await(absl::Condition(&condition_fn));
  }
}

TEST(DispatcherTest, BroadcastShouldExitGracefullyIfThereAreErrorsWithTheCode) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/5);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK(worker.Stop().ok());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/100);

  CodeObject load_bad_js_request{
      .id = "some_id",
      .version_string = "v1",
      // Bad syntax
      .js = "function test(s) { return",
  };
  absl::Notification done_loading;
  ASSERT_TRUE(dispatcher
                  .Load(std::move(load_bad_js_request),
                        [&](absl::StatusOr<ResponseObject> resp) {
                          EXPECT_FALSE(resp.ok());
                          done_loading.Notify();
                        })
                  .ok());
  done_loading.WaitForNotification();
}

TEST(DispatcherTest, DispatchBatchShouldFailIfQueuesAreFull) {
  // One worker with a one-item queue so that the queue takes long to empty out
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK(worker.Stop().ok());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/1);

  CodeObject load_request{
      .id = "some_id",
      .version_string = "v1",
      // Function that takes long so that queues will have items in it
      .js = R"""(
    function sleep(milliseconds) {
      const date = Date.now();
      let currentDate = null;
      do {
        currentDate = Date.now();
      } while (currentDate - date < milliseconds);
    }

    function takes_long() {
      sleep(2000);
      return "hello";
    }
  )""",
  };
  absl::Notification done_loading;
  ASSERT_TRUE(dispatcher
                  .Load(std::move(load_request),
                        [&](absl::StatusOr<ResponseObject> resp) {
                          EXPECT_TRUE(resp.ok());
                          done_loading.Notify();
                        })
                  .ok());
  done_loading.WaitForNotification();

  InvocationStrRequest<> execute_request = {
      .id = "some_id",
      .version_string = "v1",
      .handler_name = "takes_long",
  };
  absl::Notification finished_batch;
  ASSERT_TRUE(dispatcher
                  .Invoke(execute_request,
                          [&finished_batch](auto response) {
                            EXPECT_TRUE(response.ok());
                            finished_batch.Notify();
                          })
                  .ok());

  // This next few `Invoke`s may or may not fail depending on whether the single
  // worker has picked the first request off of the queue.
  while (!dispatcher.Invoke(execute_request, [](auto /*unused*/) {}).ok()) {
  }

  // This last `Invoke` should fail since the first is being executed and
  // occupying the worker for 2s while one of the second group is in the queue.
  EXPECT_FALSE(
      dispatcher.Invoke(std::move(execute_request), [](auto /*unused*/) {})
          .ok());
  finished_batch.WaitForNotification();
}

TEST(DispatcherTest, ShouldBeAbleToExecutePreviouslyLoadedCodeAfterCrash) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK(worker.Stop().ok());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/10);

  {
    CodeObject load_request{
        .id = "some_id",
        .version_string = "v1",
        .js = R"(test = (s) => s + " Some string";)",
    };
    absl::Notification done_loading;
    ASSERT_TRUE(dispatcher
                    .Load(std::move(load_request),
                          [&](absl::StatusOr<ResponseObject> resp) {
                            EXPECT_TRUE(resp.ok());
                            done_loading.Notify();
                          })
                    .ok());
    done_loading.WaitForNotification();
  }

  {
    InvocationStrRequest<> execute_request{
        .id = "some_id",
        .version_string = "v1",
        .handler_name = "test",
        .input = {R"("Hello")"},
    };
    absl::Notification done_executing;
    ASSERT_TRUE(dispatcher
                    .Invoke(std::move(execute_request),
                            [&](absl::StatusOr<ResponseObject> resp) {
                              EXPECT_TRUE(resp.ok());
                              EXPECT_THAT(resp->resp,
                                          StrEq(R"("Hello Some string")"));
                              done_executing.Notify();
                            })
                    .ok());
    done_executing.WaitForNotification();
  }

  // We loaded and executed successfully, so now we kill the one worker
  workers.front().Terminate();

  // This coming execution we expect will fail since the worker has died. But
  // the execution flow should cause it to be restarted.

  {
    InvocationStrRequest<> execute_request{
        .id = "some_id",
        .version_string = "v1",
        .handler_name = "test",
        .input = {R"("Hello")"},
    };

    absl::Notification done_executing;
    ASSERT_TRUE(dispatcher
                    .Invoke(std::move(execute_request),
                            [&](absl::StatusOr<ResponseObject> resp) {
                              // This execution should fail since the worker
                              // has died
                              EXPECT_FALSE(resp.ok());
                              done_executing.Notify();
                            })
                    .ok());
    done_executing.WaitForNotification();
  }

  // Now we execute again an this time around we expect it to work
  {
    InvocationStrRequest<> execute_request{
        .id = "some_id",
        .version_string = "v1",
        .handler_name = "test",
        .input = {R"JS("Hello after restart :)")JS"},
    };

    absl::Notification done_executing;
    ASSERT_TRUE(
        dispatcher
            .Invoke(std::move(execute_request),
                    [&](absl::StatusOr<ResponseObject> resp) {
                      EXPECT_TRUE(resp.ok());
                      EXPECT_THAT(
                          resp->resp,
                          StrEq(R"("Hello after restart :) Some string")"));
                      done_executing.Notify();
                    })
            .ok());
    done_executing.WaitForNotification();
  }
}

TEST(DispatcherTest, ShouldRecoverFromWorkerCrashWithMultipleCodeVersions) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK(worker.Stop().ok());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/10);

  {
    CodeObject load_request{
        .id = "some_id",
        .version_string = "v1",
        .js = R"(test = (s) => s + " Some string 1";)",
    };
    absl::Notification done_loading;
    ASSERT_TRUE(dispatcher
                    .Load(std::move(load_request),
                          [&](absl::StatusOr<ResponseObject> resp) {
                            EXPECT_TRUE(resp.ok());
                            done_loading.Notify();
                          })
                    .ok());
    done_loading.WaitForNotification();
  }

  {
    CodeObject load_request{
        .id = "some_id_2",
        .version_string = "v2",
        .js = R"(test = (s) => s + " Some string 2";)",
    };
    absl::Notification done_loading;
    ASSERT_TRUE(dispatcher
                    .Load(std::move(load_request),
                          [&](absl::StatusOr<ResponseObject> resp) {
                            EXPECT_TRUE(resp.ok());
                            done_loading.Notify();
                          })
                    .ok());
    done_loading.WaitForNotification();
  }

  // We kill the worker so we expect the first request right after to fail
  workers.front().Terminate();

  {
    InvocationStrRequest<> execute_request{
        .id = "some_id",
        .version_string = "v1",
        .handler_name = "test",
        .input = {R"("Hello")"},
    };

    absl::Notification done_executing;
    ASSERT_TRUE(dispatcher
                    .Invoke(std::move(execute_request),
                            [&](absl::StatusOr<ResponseObject> resp) {
                              // This request failed but it should have caused
                              // the restart of the worker so subsequent
                              // requests should work.
                              EXPECT_FALSE(resp.ok());
                              done_executing.Notify();
                            })
                    .ok());
    done_executing.WaitForNotification();
  }

  // Subsequent requests should succeed

  for (int i = 0; i < 10; i++) {
    {
      InvocationStrRequest<> execute_request{
          .id = "some_id",
          .version_string = "v1",
          .handler_name = "test",
          .input = {R"("Hello 1")"},
      };

      absl::Notification done_executing;
      ASSERT_TRUE(dispatcher
                      .Invoke(std::move(execute_request),
                              [&](absl::StatusOr<ResponseObject> resp) {
                                EXPECT_TRUE(resp.ok());
                                EXPECT_THAT(
                                    resp->resp,
                                    StrEq(R"("Hello 1 Some string 1")"));
                                done_executing.Notify();
                              })
                      .ok());
      done_executing.WaitForNotification();
    }
    {
      InvocationStrRequest<> execute_request{
          .id = "some_id_2",
          .version_string = "v2",
          .handler_name = "test",
          .input = {R"("Hello 2")"},
      };

      absl::Notification done_executing;
      ASSERT_TRUE(dispatcher
                      .Invoke(std::move(execute_request),
                              [&](absl::StatusOr<ResponseObject> resp) {
                                EXPECT_TRUE(resp.ok());
                                EXPECT_THAT(
                                    resp->resp,
                                    StrEq(R"("Hello 2 Some string 2")"));
                                done_executing.Notify();
                              })
                      .ok());
      done_executing.WaitForNotification();
    }
  }
}

TEST(DispatcherTest, ShouldBeAbleToLoadMoreVersionsAfterWorkerCrash) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK(worker.Stop().ok());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/10);

  {
    CodeObject load_request{
        .id = "some_id",
        .version_string = "v1",
        .js = R"(test = (s) => s + " Some string 1";)",
    };
    absl::Notification done_loading;
    ASSERT_TRUE(dispatcher
                    .Load(std::move(load_request),
                          [&](absl::StatusOr<ResponseObject> resp) {
                            EXPECT_TRUE(resp.ok());
                            done_loading.Notify();
                          })
                    .ok());
    done_loading.WaitForNotification();
  }

  {
    CodeObject load_request{
        .id = "some_id_2",
        .version_string = "v2",
        .js = R"(test = (s) => s + " Some string 2";)",
    };
    absl::Notification done_loading;
    ASSERT_TRUE(dispatcher
                    .Load(std::move(load_request),
                          [&](absl::StatusOr<ResponseObject> resp) {
                            EXPECT_TRUE(resp.ok());
                            done_loading.Notify();
                          })
                    .ok());
    done_loading.WaitForNotification();
  }

  // We kill the worker so we expect the first request right after to fail
  workers.front().Terminate();

  for (int i = 0; i < 2; i++) {
    // The first load should fail as the worker had died
    CodeObject load_request{
        .id = "some_id_3",
        .version_string = "v3",
        .js = R"(test = (s) => s + " Some string 3";)",
    };
    absl::Notification done_loading;
    ASSERT_TRUE(dispatcher
                    .Load(std::move(load_request),
                          [&](absl::StatusOr<ResponseObject> resp) {
                            if (i == 0) {
                              // Failed
                              EXPECT_FALSE(resp.ok());
                            } else {
                              EXPECT_TRUE(resp.ok());
                            }
                            done_loading.Notify();
                          })
                    .ok());
    done_loading.WaitForNotification();
  }

  // Execute all versions, those loaded before and after the worker crash
  for (int i = 0; i < 10; i++) {
    {
      InvocationStrRequest<> execute_request{
          .id = "some_id",
          .version_string = "v1",
          .handler_name = "test",
          .input = {R"("Hello 1")"},
      };

      absl::Notification done_executing;
      ASSERT_TRUE(dispatcher
                      .Invoke(std::move(execute_request),
                              [&](absl::StatusOr<ResponseObject> resp) {
                                EXPECT_TRUE(resp.ok());
                                EXPECT_THAT(resp->resp,
                                            StrEq("\"Hello 1 Some string 1\""));
                                done_executing.Notify();
                              })
                      .ok());
      done_executing.WaitForNotification();
    }
    {
      InvocationStrRequest<> execute_request{
          .id = "some_id_2",
          .version_string = "v2",
          .handler_name = "test",
          .input = {R"("Hello 2")"},
      };

      absl::Notification done_executing;
      ASSERT_TRUE(dispatcher
                      .Invoke(std::move(execute_request),
                              [&](absl::StatusOr<ResponseObject> resp) {
                                EXPECT_TRUE(resp.ok());
                                EXPECT_THAT(resp->resp,
                                            StrEq("\"Hello 2 Some string 2\""));
                                done_executing.Notify();
                              })
                      .ok());
      done_executing.WaitForNotification();
    }
    {
      InvocationStrRequest<> execute_request{
          .id = "some_id_3",
          .version_string = "v3",
          .handler_name = "test",
          .input = {R"("Hello 3")"},
      };

      absl::Notification done_executing;
      ASSERT_TRUE(dispatcher
                      .Invoke(std::move(execute_request),
                              [&](absl::StatusOr<ResponseObject> resp) {
                                EXPECT_TRUE(resp.ok());
                                EXPECT_THAT(
                                    resp->resp,
                                    StrEq(R"("Hello 3 Some string 3")"));
                                done_executing.Notify();
                              })
                      .ok());
      done_executing.WaitForNotification();
    }
  }
}

TEST(DispatcherTest, CanRunCodeWithTreatInputAsByteStr) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK(worker.Stop().ok());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/10);

  CodeObject load_request{
      .id = "some_id",
      .version_string = "v1",
      .js = "function test(input) { return input + \" Some string\"; }",
  };
  absl::Notification done_loading;
  ASSERT_TRUE(dispatcher
                  .Load(std::move(load_request),
                        [&](absl::StatusOr<ResponseObject> resp) {
                          EXPECT_TRUE(resp.ok());
                          done_loading.Notify();
                        })
                  .ok());
  done_loading.WaitForNotification();

  InvocationStrViewRequest<> execute_request{
      .id = "some_id",
      .version_string = "v1",
      .handler_name = "test",
      .input = {R"("Hello")"},
      .treat_input_as_byte_str = true,
  };
  absl::Notification done_executing;
  ASSERT_TRUE(dispatcher
                  .Invoke(std::move(execute_request),
                          [&](absl::StatusOr<ResponseObject> resp) {
                            EXPECT_TRUE(resp.ok());
                            EXPECT_THAT(resp->resp,
                                        StrEq(R"("Hello" Some string)"));
                            done_executing.Notify();
                          })
                  .ok());
  done_executing.WaitForNotification();
}

TEST(DispatcherTest, RaisesErrorWithMoreThanOneInputWithTreatInputAsByteStr) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK(worker.Stop().ok());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/1);

  CodeObject load_request{
      .id = "some_id",
      .version_string = "v1",
      .js =
          "function test(input, input2) { return input + input2 + \" Some "
          "string\"; }",
  };
  absl::Notification done_loading;
  ASSERT_TRUE(dispatcher
                  .Load(std::move(load_request),
                        [&](absl::StatusOr<ResponseObject> resp) {
                          EXPECT_TRUE(resp.ok());
                          done_loading.Notify();
                        })
                  .ok());
  done_loading.WaitForNotification();

  // Multiple inputs with treat_input_as_byte_str as true.
  InvocationStrViewRequest<> execute_request{
      .id = "some_id",
      .version_string = "v1",
      .handler_name = "test",
      .input = {R"("Hello")", R"("Hello 2")"},
      .treat_input_as_byte_str = true,
  };
  EXPECT_FALSE(dispatcher
                   .Invoke(std::move(execute_request),
                           [](absl::StatusOr<ResponseObject> resp) {})
                   .ok());
}

}  // namespace google::scp::roma::sandbox::dispatcher::test
