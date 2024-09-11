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
#include "src/roma/sandbox/dispatcher/dispatcher.h"
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

TEST(DispatcherSapiTest, ShouldBeAbleToExecutePreviouslyLoadedCodeAfterCrash) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK_OK(worker.Stop());
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
    CHECK_OK(dispatcher.Load(std::move(load_request),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               CHECK_OK(resp);
                               done_loading.Notify();
                             }));
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
    CHECK_OK(dispatcher.Invoke(
        std::move(execute_request), [&](absl::StatusOr<ResponseObject> resp) {
          CHECK_OK(resp);
          EXPECT_THAT(resp->resp, StrEq(R"("Hello Some string")"));
          done_executing.Notify();
        }));
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
    CHECK_OK(dispatcher.Invoke(std::move(execute_request),
                               [&](absl::StatusOr<ResponseObject> resp) {
                                 // This execution should fail since the worker
                                 // has died
                                 EXPECT_FALSE(resp.ok());
                                 done_executing.Notify();
                               }));
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
    CHECK_OK(dispatcher.Invoke(
        std::move(execute_request), [&](absl::StatusOr<ResponseObject> resp) {
          CHECK_OK(resp);
          EXPECT_THAT(resp->resp,
                      StrEq(R"("Hello after restart :) Some string")"));
          done_executing.Notify();
        }));
    done_executing.WaitForNotification();
  }
}

TEST(DispatcherSapiTest, ShouldRecoverFromWorkerCrashWithMultipleCodeVersions) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK_OK(worker.Stop());
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
    CHECK_OK(dispatcher.Load(std::move(load_request),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               CHECK_OK(resp);
                               done_loading.Notify();
                             }));
    done_loading.WaitForNotification();
  }

  {
    CodeObject load_request{
        .id = "some_id_2",
        .version_string = "v2",
        .js = R"(test = (s) => s + " Some string 2";)",
    };
    absl::Notification done_loading;
    CHECK_OK(dispatcher.Load(std::move(load_request),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               CHECK_OK(resp);
                               done_loading.Notify();
                             }));
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
    CHECK_OK(dispatcher.Invoke(std::move(execute_request),
                               [&](absl::StatusOr<ResponseObject> resp) {
                                 // This request failed but it should have
                                 // caused the restart of the worker so
                                 // subsequent requests should work.
                                 EXPECT_FALSE(resp.ok());
                                 done_executing.Notify();
                               }));
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
      CHECK_OK(dispatcher.Invoke(
          std::move(execute_request), [&](absl::StatusOr<ResponseObject> resp) {
            CHECK_OK(resp);
            EXPECT_THAT(resp->resp, StrEq(R"("Hello 1 Some string 1")"));
            done_executing.Notify();
          }));
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
      CHECK_OK(dispatcher.Invoke(
          std::move(execute_request), [&](absl::StatusOr<ResponseObject> resp) {
            CHECK_OK(resp);
            EXPECT_THAT(resp->resp, StrEq(R"("Hello 2 Some string 2")"));
            done_executing.Notify();
          }));
      done_executing.WaitForNotification();
    }
  }
}

TEST(DispatcherSapiTest, ShouldBeAbleToLoadMoreVersionsAfterWorkerCrash) {
  std::vector<worker_api::WorkerSandboxApi> workers =
      Workers(/*num_workers=*/1);
  absl::Cleanup cleanup = [&] {
    for (worker_api::WorkerSandboxApi& worker : workers) {
      CHECK_OK(worker.Stop());
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
    CHECK_OK(dispatcher.Load(std::move(load_request),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               CHECK_OK(resp);
                               done_loading.Notify();
                             }));
    done_loading.WaitForNotification();
  }

  {
    CodeObject load_request{
        .id = "some_id_2",
        .version_string = "v2",
        .js = R"(test = (s) => s + " Some string 2";)",
    };
    absl::Notification done_loading;
    CHECK_OK(dispatcher.Load(std::move(load_request),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               CHECK_OK(resp);
                               done_loading.Notify();
                             }));
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
    CHECK_OK(dispatcher.Load(std::move(load_request),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               if (i == 0) {
                                 // Failed
                                 EXPECT_FALSE(resp.ok());
                               } else {
                                 CHECK_OK(resp);
                               }
                               done_loading.Notify();
                             }));
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
      CHECK_OK(dispatcher.Invoke(
          std::move(execute_request), [&](absl::StatusOr<ResponseObject> resp) {
            CHECK_OK(resp);
            EXPECT_THAT(resp->resp, StrEq("\"Hello 1 Some string 1\""));
            done_executing.Notify();
          }));
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
      CHECK_OK(dispatcher.Invoke(
          std::move(execute_request), [&](absl::StatusOr<ResponseObject> resp) {
            CHECK_OK(resp);
            EXPECT_THAT(resp->resp, StrEq("\"Hello 2 Some string 2\""));
            done_executing.Notify();
          }));
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
      CHECK_OK(dispatcher.Invoke(
          std::move(execute_request), [&](absl::StatusOr<ResponseObject> resp) {
            CHECK_OK(resp);
            EXPECT_THAT(resp->resp, StrEq(R"("Hello 3 Some string 3")"));
            done_executing.Notify();
          }));
      done_executing.WaitForNotification();
    }
  }
}

}  // namespace google::scp::roma::sandbox::dispatcher::test
