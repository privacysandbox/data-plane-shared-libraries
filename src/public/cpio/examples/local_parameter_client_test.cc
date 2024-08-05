// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/synchronization/notification.h"
#include "src/public/core/interface/errors.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/parameter_client/parameter_client_interface.h"
#include "src/public/cpio/interface/type_def.h"
#include "src/public/cpio/proto/parameter_service/v1/parameter_service.pb.h"
#include "src/public/cpio/test/global_cpio/test_lib_cpio.h"

using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::ExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::cpio::LogOption;
using google::scp::cpio::ParameterClientFactory;
using google::scp::cpio::ParameterClientOptions;
using google::scp::cpio::TestCpioOptions;
using google::scp::cpio::TestLibCpio;

namespace {
constexpr std::string_view kRegion = "us-east-1";
constexpr std::string_view kTestParameterName = "test_parameter";
}  // namespace

int main(int argc, char* argv[]) {
  TestCpioOptions cpio_options{.options = {.log_option = LogOption::kConsoleLog,
                                           .region = std::string{kRegion}}};
  TestLibCpio::InitCpio(cpio_options);
  ParameterClientOptions parameter_client_options;
  auto parameter_client =
      ParameterClientFactory::Create(std::move(parameter_client_options));
  if (absl::Status error = parameter_client->Init(); !error.ok()) {
    std::cout << "Cannot init parameter client!" << error << std::endl;
    return 0;
  }
  absl::Notification finished;
  GetParameterRequest get_parameter_request;
  get_parameter_request.set_parameter_name(kTestParameterName);
  if (absl::Status error = parameter_client->GetParameter(
          std::move(get_parameter_request),
          [&](const ExecutionResult result, GetParameterResponse response) {
            if (!result.Successful()) {
              std::cout << "GetParameter failed: "
                        << GetErrorMessage(result.status_code) << std::endl;
            } else {
              std::cout << "GetParameter succeeded, and parameter is: "
                        << response.parameter_value() << std::endl;
            }
            finished.Notify();
          });
      !error.ok()) {
    std::cout << "GetParameter failed immediately: " << error << std::endl;
  }
  finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  TestLibCpio::ShutdownCpio(cpio_options);
}
