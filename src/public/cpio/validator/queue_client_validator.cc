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

#include "src/public/cpio/validator/queue_client_validator.h"

#include <memory>
#include <utility>

#include "absl/log/log.h"
#include "absl/synchronization/notification.h"
#include "src/core/interface/async_context.h"
#include "src/public/core/interface/errors.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/queue_service/v1/queue_service.pb.h"
#include "src/public/cpio/validator/proto/validator_config.pb.h"

namespace google::scp::cpio::validator {

namespace {
using google::cmrt::sdk::queue_service::v1::EnqueueMessageRequest;
using google::cmrt::sdk::queue_service::v1::EnqueueMessageResponse;
using google::cmrt::sdk::queue_service::v1::GetTopMessageRequest;
using google::cmrt::sdk::queue_service::v1::GetTopMessageResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::QueueClientOptions;
using google::scp::cpio::validator::proto::EnqueueMessageConfig;

inline constexpr std::string_view kQueueName = "queue_service_test_queue";
}  // namespace

void RunEnqueueMessageValidator(
    client_providers::CpioProviderInterface& cpio, std::string_view name,
    const EnqueueMessageConfig& enqueue_message_config) {
  if (enqueue_message_config.message_body().empty()) {
    std::cout << "[ FAILURE ]  " << name << " No message body provided."
              << std::endl;
    return;
  }
  QueueClientOptions options;
  options.queue_name = kQueueName;
  options.project_id = cpio.GetProjectId();
  auto queue_client =
      google::scp::cpio::client_providers::QueueClientProviderFactory::Create(
          std::move(options), &cpio.GetInstanceClientProvider(),
          &cpio.GetCpuAsyncExecutor(), &cpio.GetIoAsyncExecutor());
  if (!queue_client.ok()) {
    std::cout << "[ FAILURE ] " << name << " " << queue_client.status()
              << std::endl;
    return;
  }

  // EnqueueMessage.
  absl::Notification finished;
  google::scp::core::ExecutionResult result;
  auto enqueue_message_request = std::make_shared<EnqueueMessageRequest>();
  enqueue_message_request->set_message_body(
      enqueue_message_config.message_body());
  AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>
      enqueue_message_context(
          std::move(enqueue_message_request),
          [&result, &finished, &name](auto& context) {
            result = context.result;
            if (result.Successful()) {
              std::cout << "[ SUCCESS ] " << name << " " << std::endl;
              LOG(INFO) << context.response->DebugString() << std::endl;
            }
            finished.Notify();
          });
  if (absl::Status error =
          (*queue_client)->EnqueueMessage(enqueue_message_context);
      !error.ok()) {
    std::cout << "[ FAILURE ] " << name << " " << error << std::endl;
    return;
  }
  finished.WaitForNotification();
  if (!result.Successful()) {
    std::cout << "[ FAILURE ] " << name << " "
              << core::errors::GetErrorMessage(result.status_code) << std::endl;
    return;
  }
}

void RunGetTopMessageValidator(client_providers::CpioProviderInterface& cpio,
                               std::string_view name) {
  QueueClientOptions options;
  options.queue_name = kQueueName;
  options.project_id = cpio.GetProjectId();
  auto queue_client =
      google::scp::cpio::client_providers::QueueClientProviderFactory::Create(
          std::move(options), &cpio.GetInstanceClientProvider(),
          &cpio.GetCpuAsyncExecutor(), &cpio.GetIoAsyncExecutor());
  if (!queue_client.ok()) {
    std::cout << "[ FAILURE ] " << name << " " << queue_client.status()
              << std::endl;
    return;
  }

  // GetTopMessage.
  absl::Notification finished;
  google::scp::core::ExecutionResult result;
  auto get_top_message_request = std::make_shared<GetTopMessageRequest>();
  AsyncContext<GetTopMessageRequest, GetTopMessageResponse>
      get_top_message_context(
          std::move(get_top_message_request),
          [&result, &finished, &name](auto& context) {
            result = context.result;
            if (result.Successful()) {
              std::cout << "[ SUCCESS ] " << name << " " << std::endl;
              LOG(INFO) << "Message Body: " << context.response->message_body()
                        << std::endl;
            }
            finished.Notify();
          });
  if (absl::Status error =
          (*queue_client)->GetTopMessage(get_top_message_context);
      !error.ok()) {
    std::cout << "[ FAILURE ] " << name << " " << error << std::endl;
    return;
  }
  finished.WaitForNotification();
  if (!result.Successful()) {
    std::cout << "[ FAILURE ] " << name << " "
              << core::errors::GetErrorMessage(result.status_code) << std::endl;
    return;
  }
}
}  // namespace google::scp::cpio::validator
