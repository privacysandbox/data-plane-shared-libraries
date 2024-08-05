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

#include "src/public/cpio/validator/instance_client_validator.h"

#include <utility>

#include "absl/functional/bind_front.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/synchronization/notification.h"
#include "src/public/core/interface/errors.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/instance_client/instance_client_interface.h"
#include "src/public/cpio/interface/instance_client/type_def.h"
#include "src/public/cpio/proto/instance_service/v1/instance_service.pb.h"
#include "src/public/cpio/validator/proto/validator_config.pb.h"

namespace google::scp::cpio::validator {

namespace {
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse;
using google::scp::cpio::InstanceClientFactory;
using google::scp::cpio::validator::proto::GetTagsByResourceNameConfig;

void GetCurrentInstanceResourceNameCallback(
    std::string_view name, absl::Notification& finished,
    google::scp::core::ExecutionResult result,
    GetCurrentInstanceResourceNameResponse get_resource_name_response) {
  if (!result.Successful()) {
    std::cout << "[ FAILURE ] " << name << " "
              << google::scp::core::GetErrorMessage(result.status_code)
              << std::endl;
  } else {
    std::cout << "[ SUCCESS ] " << name << " " << std::endl;
    LOG(INFO) << "GetCurrentInstanceResourceName. Instance resource name: "
              << get_resource_name_response.instance_resource_name();
  }
  finished.Notify();
}

void GetTagsByResourceNameCallback(
    std::string_view name, absl::Notification& finished,
    google::scp::core::ExecutionResult result,
    GetTagsByResourceNameResponse get_tags_response) {
  if (!result.Successful()) {
    std::cout << "[ FAILURE ] " << name << " "
              << google::scp::core::GetErrorMessage(result.status_code)
              << std::endl;
  } else {
    std::cout << "[ SUCCESS ] " << name << " " << std::endl;
    LOG(INFO) << "GetTagsByResourceName. Tags: ";
    for (const auto& tag : get_tags_response.tags()) {
      LOG(INFO) << tag.first << " : " << tag.second;
    }
  }
  finished.Notify();
}
}  // namespace

void RunGetTagsByResourceNameValidator(
    std::string_view name,
    const GetTagsByResourceNameConfig& get_tags_by_resource_name_config) {
  if (get_tags_by_resource_name_config.resource_name().empty()) {
    std::cout << "[ FAILURE ] " << name << " No resource_name provided."
              << std::endl;
    return;
  }
  auto instance_client = InstanceClientFactory::Create();
  absl::Notification finished;
  GetTagsByResourceNameRequest get_tags_request;
  get_tags_request.set_resource_name(
      get_tags_by_resource_name_config.resource_name());
  if (absl::Status error = instance_client->GetTagsByResourceName(
          std::move(get_tags_request),
          absl::bind_front(&GetTagsByResourceNameCallback, name,
                           std::ref(finished)));
      !error.ok()) {
    std::cout << "[ FAILURE ] " << name << " " << error << std::endl;
  }
  finished.WaitForNotification();
}

void RunGetCurrentInstanceResourceNameValidator(std::string_view name) {
  auto instance_client = InstanceClientFactory::Create();
  absl::Notification finished;
  if (absl::Status error = instance_client->GetCurrentInstanceResourceName(
          GetCurrentInstanceResourceNameRequest(),
          absl::bind_front(&GetCurrentInstanceResourceNameCallback, name,
                           std::ref(finished)));
      !error.ok()) {
    std::cout << "[ FAILURE ] " << name << " " << error << std::endl;
  }
  finished.WaitForNotification();
}

};  // namespace google::scp::cpio::validator
