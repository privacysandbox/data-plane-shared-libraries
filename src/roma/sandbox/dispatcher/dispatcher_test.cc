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

#include <memory>
#include <string>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/types/span.h"
#include "src/roma/interface/roma.h"
#include "src/roma/sandbox/worker_api/sapi/worker_sandbox_api.h"

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
        /*enable_sandbox_sharing_request_response_with_buffer_only=*/false,
        /*v8_flags=*/std::vector<std::string>(),
        /*enable_profilers=*/false,
        /*logging_function_set=*/false,
        /*disable_udf_stacktraces_in_response=*/false);
    CHECK_OK(workers.back().Init());
    CHECK_OK(workers.back().Run());
  }
  return workers;
}
}  // namespace

TEST(DispatcherTest, CanRunCode) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK_OK(worker.Stop());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/10);

  CodeObject load_request{
      .id = "some_id",
      .version_string = "v1",
      .js = R"(function test(input) { return input + " Some string"; })",
  };
  absl::Notification done_loading;
  CHECK_OK(dispatcher.Load(std::move(load_request),
                           [&](absl::StatusOr<ResponseObject> resp) {
                             CHECK_OK(resp);
                             done_loading.Notify();
                           }));
  done_loading.WaitForNotification();

  InvocationStrRequest<> execute_request{
      .id = "some_id",
      .version_string = "v1",
      .handler_name = "test",
      .input = {R"("Hello")"},
  };
  absl::Notification done_executing;
  CHECK_OK(dispatcher.Invoke(
      std::move(execute_request), [&](absl::StatusOr<ResponseObject> resp) {
        CHECK_OK(resp);
        EXPECT_THAT(resp->resp, StrEq(R"("Hello Some string")"));
        done_executing.Notify();
      }));
  done_executing.WaitForNotification();
}

TEST(DispatcherTest, CanRunCodeMultipleWorkers) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/3);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK_OK(worker.Stop());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/10);

  CodeObject load_request{
      .id = "some_id",
      .version_string = "v1",
      .js = R"(function test(input) { return input + " Some string"; })",
  };
  absl::Notification done_loading;
  CHECK_OK(dispatcher.Load(std::move(load_request),
                           [&](absl::StatusOr<ResponseObject> resp) {
                             CHECK_OK(resp);
                             done_loading.Notify();
                           }));
  done_loading.WaitForNotification();

  absl::BlockingCounter counter(10);
  for (int i = 0; i < 10; ++i) {
    InvocationStrRequest<> execute_request{
        .id = absl::StrCat("some_id_", i),
        .version_string = "v1",
        .handler_name = "test",
        .input = {R"("Hello")"},
    };
    CHECK_OK(dispatcher.Invoke(
        std::move(execute_request), [&](absl::StatusOr<ResponseObject> resp) {
          CHECK_OK(resp);
          EXPECT_THAT(resp->resp, StrEq(R"("Hello Some string")"));
          counter.DecrementCount();
        }));
  }
  counter.Wait();
}

TEST(DispatcherTest, CanRunStringViewInputCode) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK_OK(worker.Stop());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/10);

  CodeObject load_request{
      .id = "some_id",
      .version_string = "v1",
      .js = "function test(input) { return input + \" Some string\"; }",
  };
  absl::Notification done_loading;
  CHECK_OK(dispatcher.Load(std::move(load_request),
                           [&](absl::StatusOr<ResponseObject> resp) {
                             CHECK_OK(resp);
                             done_loading.Notify();
                           }));
  done_loading.WaitForNotification();

  constexpr std::string_view kInputStrView{R"("Hello")"};
  InvocationStrViewRequest<> execute_request{
      .id = "some_id",
      .version_string = "v1",
      .handler_name = "test",
      .input = {kInputStrView},
  };
  absl::Notification done_executing;
  CHECK_OK(dispatcher.Invoke(
      std::move(execute_request), [&](absl::StatusOr<ResponseObject> resp) {
        CHECK_OK(resp);
        EXPECT_THAT(resp->resp, StrEq(R"("Hello Some string")"));
        done_executing.Notify();
      }));
  done_executing.WaitForNotification();
}

TEST(DispatcherTest, CanHandleCodeFailures) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK_OK(worker.Stop());
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
  CHECK_OK(dispatcher.Load(std::move(load_bad_js_request),
                           [&](absl::StatusOr<ResponseObject> resp) {
                             // That didn't work
                             EXPECT_FALSE(resp.ok());
                             done_loading.Notify();
                           }));
  done_loading.WaitForNotification();
}

TEST(DispatcherTest, CanHandleExecuteWithoutLoadFailure) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK_OK(worker.Stop());
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
  CHECK_OK(dispatcher.Invoke(std::move(execute_request),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               EXPECT_FALSE(resp.ok());
                               done_executing.Notify();
                             }));
  done_executing.WaitForNotification();
}

TEST(DispatcherTest, BroadcastShouldUpdateAllWorkers) {
  constexpr size_t kNumberOfWorkers = 5;
  std::vector<worker_api::WorkerSandboxApi> workers = Workers(kNumberOfWorkers);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK_OK(worker.Stop());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/100);

  CodeObject load_request{
      .id = "some_id",
      .version_string = "v1",
      .js = R"(test = (s) => s + " Some string";)",
  };
  absl::Notification done_loading;
  CHECK_OK(dispatcher.Load(std::move(load_request),
                           [&](absl::StatusOr<ResponseObject> resp) {
                             CHECK_OK(resp);
                             done_loading.Notify();
                           }));
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
    CHECK_OK(dispatcher.Invoke(std::move(execute_request),
                               [&, i](absl::StatusOr<ResponseObject> resp) {
                                 CHECK_OK(resp);
                                 EXPECT_THAT(resp->resp,
                                             absl::StrCat(R"("Hello)", i,
                                                          R"( Some string")"));
                                 absl::MutexLock lock(&execution_count_mu);
                                 execution_count++;
                               }));
  }

  {
    absl::MutexLock lock(&execution_count_mu);
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
      CHECK_OK(worker.Stop());
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
  CHECK_OK(dispatcher.Load(std::move(load_bad_js_request),
                           [&](absl::StatusOr<ResponseObject> resp) {
                             EXPECT_FALSE(resp.ok());
                             done_loading.Notify();
                           }));
  done_loading.WaitForNotification();
}

TEST(DispatcherTest, DispatchBatchShouldFailIfQueuesAreFull) {
  // One worker with a one-item queue so that the queue takes long to empty out
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK_OK(worker.Stop());
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
  CHECK_OK(dispatcher.Load(std::move(load_request),
                           [&](absl::StatusOr<ResponseObject> resp) {
                             CHECK_OK(resp);
                             done_loading.Notify();
                           }));
  done_loading.WaitForNotification();

  InvocationStrRequest<> execute_request = {
      .id = "some_id",
      .version_string = "v1",
      .handler_name = "takes_long",
  };
  absl::Notification finished_batch;
  CHECK_OK(dispatcher.Invoke(execute_request, [&finished_batch](auto response) {
    CHECK_OK(response);
    finished_batch.Notify();
  }));

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

TEST(DispatcherTest, CanRunCodeWithTreatInputAsByteStr) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK_OK(worker.Stop());
    }
  };
  Dispatcher dispatcher(absl::MakeSpan(workers), /*max_pending_reqs=*/10);

  CodeObject load_request{
      .id = "some_id",
      .version_string = "v1",
      .js = "function test(input) { return input + \" Some string\"; }",
  };
  absl::Notification done_loading;
  CHECK_OK(dispatcher.Load(std::move(load_request),
                           [&](absl::StatusOr<ResponseObject> resp) {
                             CHECK_OK(resp);
                             done_loading.Notify();
                           }));
  done_loading.WaitForNotification();

  InvocationStrViewRequest<> execute_request{
      .id = "some_id",
      .version_string = "v1",
      .handler_name = "test",
      .input = {R"("Hello")"},
      .treat_input_as_byte_str = true,
  };
  absl::Notification done_executing;
  CHECK_OK(dispatcher.Invoke(
      std::move(execute_request), [&](absl::StatusOr<ResponseObject> resp) {
        CHECK_OK(resp);
        EXPECT_THAT(resp->resp, StrEq(R"("Hello" Some string)"));
        done_executing.Notify();
      }));
  done_executing.WaitForNotification();
}

TEST(DispatcherTest, RaisesErrorWithEmptyInputWithTreatInputAsByteStr) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK_OK(worker.Stop());
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
  CHECK_OK(dispatcher.Load(std::move(load_request),
                           [&](absl::StatusOr<ResponseObject> resp) {
                             CHECK_OK(resp);
                             done_loading.Notify();
                           }));
  done_loading.WaitForNotification();

  // Empty input with treat_input_as_byte_str as true.
  InvocationStrViewRequest<> execute_request{
      .id = "some_id",
      .version_string = "v1",
      .handler_name = "test",
      .input = {},
      .treat_input_as_byte_str = true,
  };
  EXPECT_FALSE(dispatcher
                   .Invoke(std::move(execute_request),
                           [](absl::StatusOr<ResponseObject> resp) {})
                   .ok());
}

TEST(DispatcherTest, RaisesErrorWithMoreThanOneInputWithTreatInputAsByteStr) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK_OK(worker.Stop());
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
  CHECK_OK(dispatcher.Load(std::move(load_request),
                           [&](absl::StatusOr<ResponseObject> resp) {
                             CHECK_OK(resp);
                             done_loading.Notify();
                           }));
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
