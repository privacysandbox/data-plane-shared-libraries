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
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "absl/synchronization/notification.h"
#include "src/public/core/interface/errors.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/cpio.h"
#include "src/public/cpio/interface/instance_client/instance_client_interface.h"
#include "src/public/cpio/interface/type_def.h"
#include "src/public/cpio/proto/instance_service/v1/instance_service.pb.h"
#include "src/public/cpio/test/global_cpio/test_lib_cpio.h"

using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::InstanceClientFactory;
using google::scp::cpio::InstanceClientInterface;
using google::scp::cpio::InstanceClientOptions;
using google::scp::cpio::LogOption;
using google::scp::cpio::TestCpioOptions;
using google::scp::cpio::TestLibCpio;

namespace {
constexpr std::string_view kRegion = "us-east-1";
constexpr std::string_view kInstanceId = "i-1234";

void GetCurrentInstanceResourceNameCallback(
    absl::Notification& finished, ExecutionResult result,
    GetCurrentInstanceResourceNameResponse get_resource_name_response) {
  if (!result.Successful()) {
    std::cout << "Hpke encrypt failure!" << GetErrorMessage(result.status_code)
              << std::endl;
    return;
  }

  std::cout << "GetCurrentInstanceResourceName succeeded, and the "
               "instance resource name is: "
            << get_resource_name_response.instance_resource_name() << std::endl;
}
}  // namespace

int main(int argc, char* argv[]) {
  TestCpioOptions cpio_options{.instance_id = std::string{kInstanceId},
                               .options = {.log_option = LogOption::kConsoleLog,
                                           .region = std::string{kRegion}}};
  TestLibCpio::InitCpio(cpio_options);
  auto instance_client = InstanceClientFactory::Create();
  absl::Notification finished;
  if (absl::Status error = instance_client->GetCurrentInstanceResourceName(
          GetCurrentInstanceResourceNameRequest(),
          absl::bind_front(GetCurrentInstanceResourceNameCallback,
                           std::ref(finished)));
      !error.ok()) {
    std::cout << "GetCurrentInstanceResourceName failed immediately: " << error
              << std::endl;
  }
  finished.WaitForNotificationWithTimeout(absl::Seconds(3));
  TestLibCpio::ShutdownCpio(cpio_options);
}
