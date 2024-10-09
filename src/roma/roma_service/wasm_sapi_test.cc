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
#include <utility>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/log/scoped_mock_log.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"
#include "src/roma/wasm/testing_utils.h"

using google::scp::roma::sandbox::roma_service::RomaService;
using google::scp::roma::wasm::testing::WasmTestingUtils;
using ::testing::StrEq;

namespace google::scp::roma::test {
namespace {
constexpr auto kTimeout = absl::Seconds(10);
const std::vector<uint8_t> kWasmBin = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
    0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
    0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
    0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b,
};

TEST(WasmSapiTest, LoadingWasmModuleShouldFailIfMemoryRequirementIsNotMet) {
  {
    Config config;
    // This module was compiled with a memory requirement of 10MiB (160 pages
    // - each page is 64KiB). When we set the limit to 150 pages, it fails to
    // properly build the WASM object.
    config.max_wasm_memory_number_of_pages = 150;
    config.number_of_workers = 1;

    RomaService<> roma_service(std::move(config));
    ASSERT_TRUE(roma_service.Init().ok());

    auto wasm_bin = WasmTestingUtils::LoadWasmFile(
        "src/roma/testing/"
        "cpp_wasm_allocate_memory/allocate_memory.wasm");

    absl::Notification load_finished;
    {
      auto code_obj = std::make_unique<CodeObject>(CodeObject{
          .id = "foo",
          .version_string = "v1",
          .js = "",
          .wasm = std::string(reinterpret_cast<char*>(wasm_bin.data()),
                              wasm_bin.size()),
      });

      absl::Status response_status;
      ASSERT_TRUE(roma_service
                      .LoadCodeObj(std::move(code_obj),
                                   [&](absl::StatusOr<ResponseObject> resp) {
                                     response_status = resp.status();

                                     load_finished.Notify();
                                   })
                      .ok());
      ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(kTimeout));
      EXPECT_EQ(response_status.code(), absl::StatusCode::kInternal);
    }

    EXPECT_TRUE(roma_service.Stop().ok());
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

    RomaService<> roma_service(std::move(config));
    ASSERT_TRUE(roma_service.Init().ok());

    auto wasm_bin = WasmTestingUtils::LoadWasmFile(
        "src/roma/testing/"
        "cpp_wasm_allocate_memory/allocate_memory.wasm");

    absl::Notification load_finished;
    {
      auto code_obj = std::make_unique<CodeObject>(CodeObject{
          .id = "foo",
          .version_string = "v1",
          .js = "",
          .wasm = std::string(reinterpret_cast<char*>(wasm_bin.data()),
                              wasm_bin.size()),
      });

      absl::Status response;
      ASSERT_TRUE(roma_service
                      .LoadCodeObj(std::move(code_obj),
                                   [&](absl::StatusOr<ResponseObject> resp) {
                                     response = resp.status();
                                     load_finished.Notify();
                                   })
                      .ok());
      ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(kTimeout));
      // Loading works
      EXPECT_TRUE(response.ok());
    }

    EXPECT_TRUE(roma_service.Stop().ok());
  }
}

TEST(WasmSapiTest, ShouldBeAbleToExecuteJsWithWasmBinEvenAfterWorkerCrash) {
  Config config;
  // Only one worker so we can make sure it's actually restarted.
  config.number_of_workers = 1;
  // Too large an allocation will cause the worker to crash and be restarted
  // since we're giving it a max of 15 MB of heap for JS execution.
  config.ConfigureJsEngineResourceConstraints(1 /*initial_heap_size_in_mb*/,
                                              15 /*maximum_heap_size_in_mb*/);
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  absl::Notification load_finished;

  // dummy code that gets killed by the JS engine if it allocates too much
  // memory
  constexpr std::string_view js_code = R"JS_CODE(
          const module = new WebAssembly.Module(addModule);
          const instance = new WebAssembly.Instance(module);
          function Handler(a, b, c) {
            const person = {
              name: 'test',
              age: 24,
            };
            const bigObject = [];
            for (let i = 0; i < 1024*512*Number(c); i++) {
              bigObject.push(person);
            }
            return instance.exports.add(a, b);
          }
  )JS_CODE";

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = std::string(js_code),
        .wasm_bin = kWasmBin,
        .tags = {{std::string{kWasmCodeArrayName}, "addModule"}},
    });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response_status = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(kTimeout));
    ASSERT_TRUE(response_status.ok());
  }

  {
    absl::Notification execute_finished;
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            // Large input which should fail
            .input = {"1", "2", "10"},
        });

    absl::Status response_status;
    ASSERT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response_status = resp.status();
                               execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(execute_finished.WaitForNotificationWithTimeout(kTimeout));
    EXPECT_FALSE(response_status.ok());
    EXPECT_THAT(response_status.message(),
                StrEq("Sandbox worker crashed during execution "
                      "of request."));
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
            .input = {"1", "2", "1"},
        });

    absl::Status response;
    EXPECT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response = resp.status();
                               if (resp.ok()) {
                                 result = std::move(resp->resp);
                               }
                               execute_finished.Notify();
                             })
                    .ok());
    ASSERT_TRUE(execute_finished.WaitForNotificationWithTimeout(kTimeout));
    ASSERT_TRUE(response.ok());
    EXPECT_THAT(result, StrEq("3"));
  }

  EXPECT_TRUE(roma_service.Stop().ok());
}
}  // namespace
}  // namespace google::scp::roma::test
