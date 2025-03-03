// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <memory>
#include <string>

#include "absl/synchronization/notification.h"
#include "src/roma/config/config.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"
#include "src/util/duration.h"

using google::scp::roma::FunctionBindingPayload;
using google::scp::roma::sandbox::roma_service::RomaService;
using CodeObject = google::scp::roma::CodeObject;
using ResponseObject = google::scp::roma::ResponseObject;
using InvocationStrRequest = google::scp::roma::InvocationStrRequest<>;
using Config = google::scp::roma::Config<>;
using Stopwatch = privacy_sandbox::server_common::Stopwatch;

void CallbackFunction(FunctionBindingPayload<>& wrapper) {
  wrapper.io_proto.set_output_string("Hello from C++ callback! Input was: " +
                                     wrapper.io_proto.input_string());
}

int main() {
  Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(
      std::make_unique<google::scp::roma::FunctionBindingObjectV2<>>(
          google::scp::roma::FunctionBindingObjectV2<>{
              .function_name = "callback",
              .function = CallbackFunction,
          }));

  auto roma_service = std::make_unique<RomaService<>>(std::move(config));
  if (!roma_service->Init().ok()) {
    std::cerr << "Failed to initialize Roma service\n";
    return 1;
  }

  absl::Notification load_finished;
  absl::Notification execute_finished;
  std::string result;
  bool success = true;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) {
      return callback(input);
    }
    )JS_CODE",
    });

    if (!roma_service
             ->LoadCodeObj(std::move(code_obj),
                           [&](absl::StatusOr<ResponseObject> resp) {
                             if (!resp.ok()) {
                               std::cerr << "Failed to load code: "
                                         << resp.status().message() << "\n";
                               success = false;
                             }
                             load_finished.Notify();
                           })
             .ok()) {
      std::cerr << "Failed to submit code load request\n";
      return 1;
    }
  }

  load_finished.WaitForNotification();
  if (!success) {
    return 1;
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest>(InvocationStrRequest{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("foobar")"},
        });

    Stopwatch stopwatch;
    if (!roma_service
             ->Execute(std::move(execution_obj),
                       [&](absl::StatusOr<ResponseObject> resp) {
                         if (!resp.ok()) {
                           std::cerr << "Execution failed: "
                                     << resp.status().message() << "\n";
                           success = false;
                         } else {
                           result = std::move(resp->resp);
                         }
                         execute_finished.Notify();
                       })
             .ok()) {
      std::cerr << "Failed to submit execution request\n";
      return 1;
    }

    execute_finished.WaitForNotification();
    std::cout << "Execution duration: " << stopwatch.GetElapsedTime()
              << std::endl;

    if (!success) {
      return 1;
    }

    std::cout << "Execution result: " << result << std::endl;
  }

  if (!roma_service->Stop().ok()) {
    std::cerr << "Failed to stop Roma service\n";
    return 1;
  }

  return 0;
}
