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

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/synchronization/notification.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "public/core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/cpio.h"
#include "public/cpio/interface/parameter_client/parameter_client_interface.h"
#include "public/cpio/interface/type_def.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::GetErrorMessage;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::LogOption;
using google::scp::cpio::ParameterClientFactory;
using google::scp::cpio::ParameterClientInterface;
using google::scp::cpio::ParameterClientOptions;
using google::scp::cpio::client_providers::GlobalCpio;

ABSL_FLAG(std::string, project_id, "test-project", "GCP Project ID");
ABSL_FLAG(std::string, parameter_name, "test_parameter_name",
          "The name of the parameter to be retrieved");
ABSL_FLAG(std::string, parameter_value, "test_parameter_value",
          "The value of the parameter to be retrieved");

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  absl::ParseCommandLine(argc, argv);
  absl::SetStderrThreshold(absl::LogSeverity::kInfo);

  CpioOptions cpio_options = {};
  cpio_options.log_option = LogOption::kConsoleLog;
  cpio_options.project_id = absl::GetFlag(FLAGS_project_id);
  auto status = Cpio::InitCpio(cpio_options);
  if (!status.Successful()) {
    LOG(FATAL) << "Failed to initialize CPIO. "
               << GetErrorMessage(status.status_code);
  }

  ParameterClientOptions parameter_client_options;
  auto parameter_client =
      ParameterClientFactory::Create(std::move(parameter_client_options));

  status = parameter_client->Init();
  if (!status.Successful()) {
    LOG(FATAL) << "Cannot init parameter client."
               << GetErrorMessage(status.status_code);
  }

  status = parameter_client->Run();
  if (!status.Successful()) {
    LOG(FATAL) << "Cannot run parameter client."
               << GetErrorMessage(status.status_code);
  }

  GetParameterRequest get_parameter_request;
  get_parameter_request.set_parameter_name(absl::GetFlag(FLAGS_parameter_name));

  absl::Notification done;
  status = parameter_client->GetParameter(
      std::move(get_parameter_request),
      [&](const ExecutionResult result, GetParameterResponse response) {
        EXPECT_TRUE(result.Successful());
        if (!result.Successful()) {
          LOG(FATAL) << "GetParameter failed: "
                     << GetErrorMessage(result.status_code);
        } else if (response.parameter_value() !=
                   absl::GetFlag(FLAGS_parameter_value)) {
          LOG(FATAL) << "GetParameter succeeded with incorrect parameter: "
                     << response.parameter_value()
                     << " . Expected parameter is boat";
        } else {
          LOG(INFO) << "GetParameter succeeded with parameter: "
                    << response.parameter_value();
        }
        done.Notify();
      });
  if (!status.Successful()) {
    LOG(FATAL) << "GetParameter failed immediately."
               << GetErrorMessage(status.status_code);
  }

  done.WaitForNotification();

  status = parameter_client->Stop();
  if (!status.Successful()) {
    LOG(FATAL) << "Cannot stop parameter client."
               << GetErrorMessage(status.status_code);
  }

  status = Cpio::ShutdownCpio(cpio_options);
  if (!status.Successful()) {
    LOG(FATAL) << "Failed to shutdown CPIO."
               << GetErrorMessage(status.status_code);
  }
}
