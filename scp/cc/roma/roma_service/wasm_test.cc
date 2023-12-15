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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/log/scoped_mock_log.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "roma/config/src/config.h"
#include "roma/interface/roma.h"
#include "roma/roma_service/roma_service.h"
#include "roma/wasm/test/testing_utils.h"

using google::scp::roma::sandbox::roma_service::RomaService;
using google::scp::roma::wasm::testing::WasmTestingUtils;
using ::testing::StrEq;

namespace google::scp::roma::test {
static const std::vector<uint8_t> kWasmBin = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
    0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
    0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
    0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b,
};

TEST(WasmTest, CanExecuteWasmCode) {
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
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_THAT(result, StrEq(R"("Foobar Hello World from WASM")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(WasmTest, CanLogFromInlineWasmCode) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  absl::ScopedMockLog log;
  EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, testing::_, "LOG_STRING"));
  EXPECT_CALL(log, Log(absl::LogSeverity::kError, testing::_, "ERR_STRING"));
  log.StartCapturingLogs();
  {
    const std::string inline_wasm_js = WasmTestingUtils::LoadJsWithWasmFile(
        "./scp/cc/roma/testing/cpp_wasm_hello_world_example/"
        "hello_world_udf_generated.js");

    const std::string udf = R"(
      async function HandleRequest(input, log_input, err_input) {
        roma.n_log("JS HandleRequest START");
        const module = await getModule();
        roma.n_log("WASM loaded");

        const result = module.HelloClass.SayHello(input, log_input,
        err_input);
        roma.n_log("C++ result: " + result);
        return result;
      }
    )";

    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = absl::StrCat(inline_wasm_js, udf),
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
            .handler_name = "HandleRequest",
        });
    execution_obj->input.push_back(R"("Foobar")");
    execution_obj->input.push_back(R"("LOG_STRING")");
    execution_obj->input.push_back(R"("ERR_STRING")");

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
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_THAT(result, StrEq(R"("Hello from C++! Input: Foobar")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());

  log.StopCapturingLogs();
}

TEST(WasmTest, LoadingWasmModuleShouldFailIfMemoryRequirementIsNotMet) {
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

TEST(WasmTest, CanExecuteJSWithWasmCode) {
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
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_THAT(result, StrEq("3"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(WasmTest, LoadJSWithWasmCodeShouldFailOnInvalidRequest) {
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

  ASSERT_TRUE(load_finished1.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(load_finished2.WaitForNotificationWithTimeout(absl::Seconds(10)));
  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

// TODO(b/311435456): Reenable when cause of flakiness found.
TEST(WasmTest, DISABLED_CanExecuteJSWithWasmCodeWithStandaloneJS) {
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
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_THAT(result, StrEq("3"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

TEST(WasmTest, ShouldBeAbleToExecuteJsWithWasmBinEvenAfterWorkerCrash) {
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
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));

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
    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
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

    ASSERT_TRUE(
        execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));

    EXPECT_THAT(result, StrEq("3"));
  }

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

}  // namespace google::scp::roma::test
