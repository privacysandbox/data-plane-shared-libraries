// Copyright 2024 Google LLC
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
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"
#include "src/util/duration.h"

using google::scp::roma::sandbox::roma_service::RomaService;
using CodeObject = google::scp::roma::CodeObject;
using ResponseObject = google::scp::roma::ResponseObject;
using InvocationStrRequest = google::scp::roma::InvocationStrRequest<>;
using Config = google::scp::roma::Config<>;
using Stopwatch = privacy_sandbox::server_common::Stopwatch;

int main() {
  // Create a basic Roma service configuration
  Config config;
  config.number_of_workers = 2;

  // Initialize the Roma service as a unique_ptr
  auto roma_service = std::make_unique<RomaService<>>(std::move(config));
  if (!roma_service->Init().ok()) {
    std::cerr << "Failed to initialize Roma service\n";
    return 1;
  }

  absl::Notification load_finished;
  absl::Notification execute_finished;
  std::string result;
  bool success = true;

  // Load the JavaScript code
  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "hello",
        .version_string = "v1",
        .js = R"JS_CODE(
    function Handler(input) {
      return {
        success: true,
        data: "CjwKFAoIbWV0YWRhdGESCAoGCgR0ZXN0ChMKB3Byb2R1Y3QSCBIGCgRI4QNDCg8KA3N1bRIIEgYKBFyPNEI=",
      }
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

  // Wait for code load to complete
  load_finished.WaitForNotification();
  if (!success) {
    return 1;
  }

  // Execute the loaded code with timing
  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest>(InvocationStrRequest{
            .id = "hello",
            .version_string = "v1",
            .handler_name = "Handler",
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

    // Wait for execution to complete
    execute_finished.WaitForNotification();
    std::cout << "Execution duration: " << stopwatch.GetElapsedTime()
              << std::endl;

    if (!success) {
      return 1;
    }

    // Print the result and execution time
    std::cout << "Execution result: " << result << std::endl;
  }

  // Cleanup
  if (!roma_service->Stop().ok()) {
    std::cerr << "Failed to stop Roma service\n";
    return 1;
  }

  return 0;
}
