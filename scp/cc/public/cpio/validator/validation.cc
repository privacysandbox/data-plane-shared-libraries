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
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <utility>

#include <google/protobuf/text_format.h>

#include "absl/base/log_severity.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/flags.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "core/common/time_provider/src/time_provider.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "public/core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/cpio.h"
#include "scp/cc/public/cpio/validator/blob_storage_client_validator.h"
#include "scp/cc/public/cpio/validator/instance_client_validator.h"
#include "scp/cc/public/cpio/validator/parameter_client_validator.h"
#include "scp/cc/public/cpio/validator/proto/validator_config.pb.h"
#include "scp/cc/public/cpio/validator/queue_client_validator.h"

namespace {

using google::scp::core::AsyncContext;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::errors::GetErrorMessage;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::validator::proto::TestCase;
using google::scp::cpio::validator::proto::ValidatorConfig;

constexpr absl::Duration kRequestTimeout = absl::Seconds(10);
const char kValidatorConfigPath[] = "/etc/validator_config.txtpb";
const char kHostName[] = "www.google.com";
}  // namespace

std::string_view GetValidatorFailedToRunMsg() {
  return "[ FAILURE ] Could not run all validation tests. For details, see "
         "above.";
}

void RunDnsLookupValidator(absl::Notification& finished, bool& successful) {
  const struct addrinfo hints = {
      .ai_family = AF_INET,
      .ai_socktype = SOCK_STREAM,
  };
  struct addrinfo* addr_infos;
  if (int err_code = getaddrinfo(kHostName, "80", &hints, &addr_infos);
      err_code != 0) {
    std::cout << "[ FAILURE ] getaddrinfo: " << gai_strerror(err_code)
              << ". Check /etc/resolv.conf. Verify if proxy is running."
              << std::endl;
    successful = false;
    finished.Notify();
    return;
  }
  std::cout << "[ SUCCESS ] DNS lookup succeeded for host " << kHostName
            << std::endl;
  for (auto rp = addr_infos; rp != nullptr; rp = rp->ai_next) {
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];

    getnameinfo(rp->ai_addr, rp->ai_addrlen, host, NI_MAXHOST, service,
                NI_MAXSERV, NI_NUMERICHOST);
    LOG(INFO) << "host: " << host << "\t\t service: " << service;
  }
  freeaddrinfo(addr_infos);
  successful = true;
  finished.Notify();
}

google::scp::core::ExecutionResult MakeRequest(
    google::scp::core::HttpClientInterface& http_client,
    google::scp::core::HttpMethod method, const std::string& url,
    const absl::btree_multimap<std::string, std::string>& headers = {}) {
  auto request = std::make_shared<HttpRequest>();
  request->method = method;
  request->path = std::make_shared<std::string>(url);
  if (!headers.empty()) {
    request->headers =
        std::make_shared<google::scp::core::HttpHeaders>(headers);
  }
  google::scp::core::ExecutionResult context_result;
  absl::Notification finished;
  AsyncContext<HttpRequest, HttpResponse> context(
      std::move(request),
      [&](AsyncContext<HttpRequest, HttpResponse>& context) {
        context_result = context.result;
        finished.Notify();
      });

  if (auto result = http_client.PerformRequest(context, kRequestTimeout);
      !result.Successful()) {
    return result;
  }
  finished.WaitForNotification();
  return context_result;
}

void RunProxyValidator() {
  std::shared_ptr<google::scp::core::HttpClientInterface> http_client;
  if (auto result = GlobalCpio::GetGlobalCpio()->GetHttp1Client(http_client);
      !result.Successful()) {
    std::cout << "[ FAILURE ] Unable to get Http Client." << std::endl;
    return;
  }

  http_client->Init();
  http_client->Run();

  if (!MakeRequest(*http_client, google::scp::core::HttpMethod::GET,
                   "https://www.google.com/")
           .Successful()) {
    std::cout
        << "[ FAILURE ] Could not connect to outside world. Check if proxy "
           "is running."
        << std::endl;
  } else {
    std::cout << "[ SUCCESS ] Connected to outside world." << std::endl;
  }

  if (!MakeRequest(*http_client, google::scp::core::HttpMethod::PUT,
                   "http://169.254.169.254/latest/api/token",
                   {{"X-aws-ec2-metadata-token-ttl-seconds", "21600"}})
           .Successful()) {
    std::cout << "[ FAILURE ] Could not access AWS resource. Check if proxy is "
                 "running."
              << std::endl;
  } else {
    std::cout << "[ SUCCESS ] Accessed AWS resource." << std::endl;
  }

  http_client->Stop();
}

int main(int argc, char* argv[]) {
  // Process command line parameters
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();

  absl::Notification finished;
  bool successful;
  std::thread dns_lookup_thread(&RunDnsLookupValidator, std::ref(finished),
                                std::ref(successful));
  dns_lookup_thread.detach();
  if (!finished.WaitForNotificationWithTimeout(kRequestTimeout)) {
    std::cout << "[ FAILURE ] DNS lookup timed out." << std::endl;
    successful = false;
  }
  if (!successful) {
    GetValidatorFailedToRunMsg();
    return -1;
  }

  ValidatorConfig validator_config;
  int fd = open(kValidatorConfigPath, O_RDONLY);
  if (fd < 0) {
    std::cout << "[ FAILURE ] Unable to open validator config file."
              << std::endl;
    std::cout << GetValidatorFailedToRunMsg() << std::endl;
    return -1;
  }
  google::protobuf::io::FileInputStream file_input_stream(fd);
  file_input_stream.SetCloseOnDelete(true);

  if (!google::protobuf::TextFormat::Parse(&file_input_stream,
                                           &validator_config)) {
    std::cout << std::endl
              << "[ FAILURE ] Unable to parse validator config file."
              << std::endl;
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
    std::cout << "[ FAILURE ] Unable to initialize CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
    std::cout << GetValidatorFailedToRunMsg() << std::endl;
    return -1;
  }
  RunProxyValidator();
  for (auto test_case : validator_config.test_cases()) {
    switch (test_case.client_config_case()) {
      case TestCase::ClientConfigCase::kGetTagsByResourceNameConfig:
        google::scp::cpio::validator::RunGetTagsByResourceNameValidator(
            test_case.name(), test_case.get_tags_by_resource_name_config());
        break;
      case TestCase::ClientConfigCase::kGetCurrentInstanceResourceNameConfig:
        google::scp::cpio::validator::
            RunGetCurrentInstanceResourceNameValidator(test_case.name());
        break;
      case TestCase::ClientConfigCase::kGetBlobConfig:
        google::scp::cpio::validator::RunGetBlobValidator(
            test_case.name(), test_case.get_blob_config());
        break;
      case TestCase::ClientConfigCase::kListBlobsMetadataConfig:
        google::scp::cpio::validator::RunListBlobsMetadataValidator(
            test_case.name(), test_case.list_blobs_metadata_config());
        break;
      case TestCase::ClientConfigCase::kGetParameterConfig:
        google::scp::cpio::validator::RunGetParameterValidator(
            test_case.name(), test_case.get_parameter_config());
        break;
      case TestCase::ClientConfigCase::kEnqueueMessageConfig:
        google::scp::cpio::validator::RunEnqueueMessageValidator(
            test_case.name(), test_case.enqueue_message_config());
        break;
      case TestCase::ClientConfigCase::kGetTopMessageConfig:
        google::scp::cpio::validator::RunGetTopMessageValidator(
            test_case.name());
        break;
      default:
        std::cout << "[ FAILURE ] UNEXPECTED INPUT" << std::endl;
        break;
    }
  }
  std::cout << "Ran all validation tests. For individual statuses, "
               "see above."
            << std::endl;
  return 0;
}
