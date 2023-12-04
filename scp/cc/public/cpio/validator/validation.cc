// Copyright 2023 Google LLC
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

#include <fcntl.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string_view>
#include <utility>

#include <google/protobuf/text_format.h>

#include "absl/base/log_severity.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/flags.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "public/core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/cpio.h"
#include "scp/cc/public/cpio/validator/blob_storage_client_validator.h"
#include "scp/cc/public/cpio/validator/instance_client_validator.h"
#include "scp/cc/public/cpio/validator/parameter_client_validator.h"
#include "scp/cc/public/cpio/validator/proto/validator_config.pb.h"

namespace {

using google::scp::core::errors::GetErrorMessage;
using google::scp::cpio::validator::BlobStorageClientValidator;
using google::scp::cpio::validator::InstanceClientValidator;
using google::scp::cpio::validator::ParameterClientValidator;
using google::scp::cpio::validator::proto::ValidatorConfig;

constexpr char kValidatorConfigPath[] = "/etc/validator_config.txtpb";
}  // namespace

std::string_view GetValidatorFailedToRunMsg() {
  return "FAILURE. Could not run all validation tests. For details, see above.";
}

int main(int argc, char* argv[]) {
  // Process command line parameters
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();

  ValidatorConfig validator_config;
  int fd = open(kValidatorConfigPath, O_RDONLY);
  if (fd < 0) {
    std::cout << "FAILURE. Unable to open validator config file." << std::endl;
    std::cout << GetValidatorFailedToRunMsg() << std::endl;
    return -1;
  }
  google::protobuf::io::FileInputStream file_input_stream(fd);
  file_input_stream.SetCloseOnDelete(true);

  if (!google::protobuf::TextFormat::Parse(&file_input_stream,
                                           &validator_config)) {
    std::cout << std::endl
              << "FAILURE. Unable to parse validator config file." << std::endl;
    std::cout << GetValidatorFailedToRunMsg() << std::endl;
    return -1;
  }
  google::scp::cpio::CpioOptions cpio_options;
  cpio_options.log_option =
      (absl::StderrThreshold() == absl::LogSeverityAtLeast::kInfo)
          ? google::scp::cpio::LogOption::kConsoleLog
          : google::scp::cpio::LogOption::kNoLog;

  if (google::scp::core::ExecutionResult result =
          google::scp::cpio::Cpio::InitCpio(cpio_options);
      !result.Successful()) {
    std::cout << "FAILURE. Unable to initialize CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
    std::cout << GetValidatorFailedToRunMsg() << std::endl;
    return -1;
  }
  if (!validator_config.skip_instance_client_validation()) {
    InstanceClientValidator instance_client_validator;
    instance_client_validator.Run();
  }
  if (validator_config.has_parameter_client_config()) {
    ParameterClientValidator parameter_client_validator;
    parameter_client_validator.Run(validator_config.parameter_client_config());
  }
  if (validator_config.has_blob_storage_client_config()) {
    BlobStorageClientValidator blob_storage_client_validator;
    blob_storage_client_validator.Run(
        validator_config.blob_storage_client_config());
  }
  std::cout << "SUCCESS. Ran all validation tests. For individual statuses, "
               "see above."
            << std::endl;
  return 0;
}
