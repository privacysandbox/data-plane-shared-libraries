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

#include "scp/cc/public/cpio/validator/instance_client_validator.h"

#include <utility>

#include "absl/functional/bind_front.h"
#include "absl/log/log.h"
#include "absl/synchronization/notification.h"
#include "public/core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "scp/cc/public/core/interface/execution_result.h"
#include "scp/cc/public/cpio/interface/instance_client/instance_client_interface.h"
#include "scp/cc/public/cpio/interface/instance_client/type_def.h"
#include "scp/cc/public/cpio/proto/instance_service/v1/instance_service.pb.h"

namespace google::scp::cpio::validator {

namespace {

using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse;
using google::scp::cpio::InstanceClientFactory;
}  // namespace

InstanceClientValidator::InstanceClientValidator() {
  google::scp::cpio::InstanceClientOptions instance_client_options;
  instance_client_ = InstanceClientFactory::Create(instance_client_options);
}

void InstanceClientValidator::Run() {
  if (google::scp::core::ExecutionResult result = instance_client_->Init();
      !result.Successful()) {
    std::cout << "FAILURE. Could not Init instance_client_. "
              << google::scp::core::GetErrorMessage(result.status_code)
              << std::endl;
    return;
  }

  if (google::scp::core::ExecutionResult result = instance_client_->Run();
      !result.Successful()) {
    std::cout << "FAILURE. Could not Run instance_client_. "
              << google::scp::core::GetErrorMessage(result.status_code)
              << std::endl;
    return;
  }

  absl::Notification finished;
  google::scp::core::ExecutionResult result =
      instance_client_->GetCurrentInstanceResourceName(
          GetCurrentInstanceResourceNameRequest(),
          absl::bind_front(
              &InstanceClientValidator::GetCurrentInstanceResourceNameCallback,
              this, std::ref(finished)));

  if (!result.Successful()) {
    std::cout << "FAILURE. GetCurrentInstanceResourceName failed. "
              << google::scp::core::GetErrorMessage(result.status_code)
              << std::endl;
  }
  finished.WaitForNotificationWithTimeout(absl::Seconds(3));

  if (google::scp::core::ExecutionResult result = instance_client_->Stop();
      !result.Successful()) {
    std::cout << "FAILURE. Could not Stop instance_client_. "
              << google::scp::core::GetErrorMessage(result.status_code)
              << std::endl;
  }
}

void InstanceClientValidator::GetTagsByResourceNameCallback(
    absl::Notification& finished, google::scp::core::ExecutionResult result,
    GetTagsByResourceNameResponse get_tags_response) {
  if (!result.Successful()) {
    std::cout << "FAILURE. GetTagsByResourceName failed. "
              << google::scp::core::GetErrorMessage(result.status_code)
              << std::endl;
  } else {
    std::cout << "SUCCESS. GetTagsByResourceName succeeded." << std::endl;
    LOG(INFO) << "GetTagsByResourceName. Tags: ";
    for (const auto& tag : get_tags_response.tags()) {
      LOG(INFO) << tag.first << " : " << tag.second;
    }
  }
  finished.Notify();
}

void InstanceClientValidator::GetCurrentInstanceResourceNameCallback(
    absl::Notification& finished, google::scp::core::ExecutionResult result,
    GetCurrentInstanceResourceNameResponse get_resource_name_response) {
  if (!result.Successful()) {
    std::cout << "FAILURE. Hpke encrypt failed. "
              << google::scp::core::GetErrorMessage(result.status_code)
              << std::endl;
    return;
  }

  std::cout << "SUCCESS. GetCurrentInstanceResourceName succeeded."
            << std::endl;
  LOG(INFO) << "GetCurrentInstanceResourceName. Instance resource name: "
            << get_resource_name_response.instance_resource_name();

  GetTagsByResourceNameRequest get_tags_request;
  get_tags_request.set_resource_name(
      get_resource_name_response.instance_resource_name());
  result = instance_client_->GetTagsByResourceName(
      std::move(get_tags_request),
      absl::bind_front(&InstanceClientValidator::GetTagsByResourceNameCallback,
                       this, std::ref(finished)));
  if (!result.Successful()) {
    std::cout << "FAILURE. GetTagsByResourceName failed. "
              << google::scp::core::GetErrorMessage(result.status_code)
              << std::endl;
  }
}
};  // namespace google::scp::cpio::validator
