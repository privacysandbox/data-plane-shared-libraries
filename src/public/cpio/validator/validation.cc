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
#include "absl/time/time.h"
#include "src/core/common/time_provider/time_provider.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/cpio/client_providers/interface/cpio_provider_interface.h"
#include "src/public/core/interface/errors.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/cpio.h"
#include "src/public/cpio/validator/blob_storage_client_validator.h"
#include "src/public/cpio/validator/instance_client_validator.h"
#include "src/public/cpio/validator/key_fetcher_validator.h"
#include "src/public/cpio/validator/parameter_client_validator.h"
#include "src/public/cpio/validator/proto/validator_config.pb.h"
#include "src/public/cpio/validator/queue_client_validator.h"

namespace {

using google::scp::core::AsyncContext;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::errors::GetErrorMessage;
using google::scp::cpio::client_providers::CpioProviderInterface;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::validator::proto::TestCase;
using google::scp::cpio::validator::proto::ValidatorConfig;

constexpr absl::Duration kRequestTimeout = absl::Seconds(10);
constexpr std::string_view kValidatorConfigPath = "/etc/validator_config.txtpb";
}  // namespace

std::string_view GetValidatorFailedToRunMsg() {
  return "[ FAILURE ] Could not run all validation tests. For details, see "
         "above.";
}

void RunDnsLookupValidator(
    std::string_view name,
    const google::scp::cpio::validator::proto::DnsConfig& dns_config) {
  const struct addrinfo hints = {
      .ai_family = AF_INET,
      .ai_socktype = SOCK_STREAM,
  };
  struct addrinfo* addr_infos;
  if (int err_code = getaddrinfo(dns_config.host().c_str(),
                                 std::to_string(dns_config.port()).c_str(),
                                 &hints, &addr_infos);
      err_code != 0) {
    std::cout << "[ FAILURE ] " << name
              << " getaddrinfo: " << gai_strerror(err_code)
              << ". Check /etc/resolv.conf. Verify if proxy is running."
              << std::endl;
    return;
  }
  std::cout << "[ SUCCESS ] " << name << " DNS lookup succeeded for host "
            << dns_config.host() << std::endl;
  for (auto rp = addr_infos; rp != nullptr; rp = rp->ai_next) {
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];

    getnameinfo(rp->ai_addr, rp->ai_addrlen, host, NI_MAXHOST, service,
                NI_MAXSERV, NI_NUMERICHOST);
    LOG(INFO) << "host: " << host << "\t\t service: " << service;
  }
  freeaddrinfo(addr_infos);
}

google::scp::core::ExecutionResult MakeRequest(
    google::scp::core::HttpClientInterface& http_client,
    google::scp::core::HttpMethod method, std::string_view url,
    const google::protobuf::Map<std::string, std::string>& headers = {}) {
  absl::btree_multimap<std::string, std::string> btree_headers;
  for (const auto& entry : headers) {
    btree_headers.insert({std::string(entry.first), std::string(entry.second)});
  }
  auto request = std::make_shared<HttpRequest>();
  request->method = method;
  request->path = std::make_shared<std::string>(url);
  if (!headers.empty()) {
    request->headers =
        std::make_shared<google::scp::core::HttpHeaders>(btree_headers);
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

void RunHttpValidator(
    CpioProviderInterface& cpio, std::string_view name,
    const google::scp::cpio::validator::proto::HttpConfig& http_config) {
  absl::flat_hash_map<std::string, google::scp::core::HttpMethod>
      http_method_map = {{"GET", google::scp::core::HttpMethod::GET},
                         {"POST", google::scp::core::HttpMethod::POST},
                         {"PUT", google::scp::core::HttpMethod::PUT}};
  if (http_method_map.find(http_config.request_method()) ==
      http_method_map.end()) {
    std::cout << "[ FAILURE ] " << name
              << " Http request method: " << http_config.request_method()
              << " is invalid." << std::endl;
  }

  if (!MakeRequest(cpio.GetHttp1Client(),
                   http_method_map[http_config.request_method()],
                   http_config.request_url(), http_config.request_headers())
           .Successful()) {
    std::cout << "[ FAILURE ] " << name
              << " Could not connect to request URL. Check if proxy "
                 "is running."
              << std::endl;
  } else {
    std::cout << "[ SUCCESS ] " << name << " Connected to request URL."
              << std::endl;
  }
}

int main(int argc, char* argv[]) {
  // Process command line parameters
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();

  ValidatorConfig validator_config;
  int fd = open(kValidatorConfigPath.data(), O_RDONLY);
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
  CpioProviderInterface& cpio = GlobalCpio::GetGlobalCpio();

  // Run test cases for DNS and HTTP Proxy first.
  for (auto test_case : validator_config.test_cases()) {
    switch (test_case.client_config_case()) {
      case TestCase::ClientConfigCase::kDnsConfig:
        RunDnsLookupValidator(test_case.name(), test_case.dns_config());
        break;
      case TestCase::ClientConfigCase::kHttpConfig:
        RunHttpValidator(cpio, test_case.name(), test_case.http_config());
        break;
      default:
        break;
    }
  }

  // Run test cases for CPIO components.
  for (auto test_case : validator_config.test_cases()) {
    switch (test_case.client_config_case()) {
      case TestCase::ClientConfigCase::kDnsConfig:
      case TestCase::ClientConfigCase::kHttpConfig:
        break;
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
            cpio, test_case.name(), test_case.enqueue_message_config());
        break;
      case TestCase::ClientConfigCase::kGetTopMessageConfig:
        google::scp::cpio::validator::RunGetTopMessageValidator(
            cpio, test_case.name());
        break;
      case TestCase::ClientConfigCase::kFetchPrivateKeyConfig:
        google::scp::cpio::validator::RunFetchPrivateKeyValidator(
            cpio, test_case.name(), test_case.fetch_private_key_config());
        break;
      default:
        std::cout << "[ FAILURE ] UNEXPECTED INPUT" << std::endl;
        break;
    }
  }
  std::cout << "Ran all validation tests. For individual statuses, "
               "see above."
            << std::endl;
  if (google::scp::core::ExecutionResult result =
          google::scp::cpio::Cpio::ShutdownCpio(cpio_options);
      !result.Successful()) {
    std::cout << "[ FAILURE ] Unable to shutdown CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
    std::cout << GetValidatorFailedToRunMsg() << std::endl;
    return -1;
  }
  return 0;
}
