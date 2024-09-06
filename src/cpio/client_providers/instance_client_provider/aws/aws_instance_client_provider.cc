/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "aws_instance_client_provider.h"

#include <memory>
#include <string>
#include <utility>

#include <aws/ec2/model/DescribeInstancesRequest.h>
#include <aws/ec2/model/DescribeTagsRequest.h>
#include <nlohmann/json.hpp>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/bind_front.h"
#include "absl/strings/substitute.h"
#include "src/core/async_executor/aws/aws_async_executor.h"
#include "src/core/common/global_logger/global_logger.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/interface/http_client_interface.h"
#include "src/cpio/common/aws/aws_utils.h"
#include "src/cpio/common/cpio_utils.h"
#include "src/public/core/interface/execution_result.h"

#include "aws_instance_client_utils.h"
#include "ec2_error_converter.h"
#include "error_codes.h"

using Aws::Client::AsyncCallerContext;
using Aws::EC2::EC2Client;
using Aws::EC2::Model::DescribeInstancesOutcome;
using Aws::EC2::Model::DescribeInstancesRequest;
using Aws::EC2::Model::DescribeTagsOutcome;
using Aws::EC2::Model::DescribeTagsRequest;
using Aws::EC2::Model::Filter;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::InstanceDetails;
using google::cmrt::sdk::instance_service::v1::InstanceNetwork;
using google::cmrt::sdk::instance_service::v1::
    ListInstanceDetailsByEnvironmentRequest;
using google::cmrt::sdk::instance_service::v1::
    ListInstanceDetailsByEnvironmentResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_AWS_INSTANCE_CLIENT_INSTANCE_RESOURCE_NAME_RESPONSE_MALFORMED;
using google::scp::core::errors::SC_AWS_INSTANCE_CLIENT_INVALID_REGION_CODE;
using google::scp::core::errors::
    SC_AWS_INSTANCE_CLIENT_PROVIDER_DESCRIBE_INSTANCES_RESPONSE_MALFORMED;
using google::scp::cpio::common::CpioUtils;
using google::scp::cpio::common::CreateClientConfiguration;
using nlohmann::json;

namespace {
constexpr std::string_view kAwsInstanceClientProvider =
    "AwsInstanceClientProvider";
constexpr std::string_view kAuthorizationHeaderKey = "X-aws-ec2-metadata-token";
constexpr size_t kMaxConcurrentConnections = 1000;
// Resource ID tag name.
constexpr std::string_view kResourceIdFilterName = "resource-id";
// Use IMDSv2 to fetch current instance ID.
// For more information, see
// https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/configuring-instance-metadata-service.html
constexpr std::string_view kAwsInstanceDynamicDataUrl =
    "http://169.254.169.254/latest/dynamic/instance-identity/document";
constexpr std::string_view kAccountIdKey = "accountId";
constexpr std::string_view kInstanceIdKey = "instanceId";
constexpr std::string_view kRegionKey = "region";
constexpr std::string_view kAwsInstanceResourceNameFormat =
    "arn:aws:ec2:$0:$1:instance/$2";
constexpr std::string_view kDefaultRegionCode = "us-east-1";

// Returns a pair of iterators - one to the beginning, one to the end.
const auto& GetRequiredFieldsForInstanceDynamicData() {
  static char const* components[3];
  using iterator_type = decltype(std::cbegin(components));
  static std::pair<iterator_type, iterator_type> iterator_pair = []() {
    components[0] = kAccountIdKey.data();
    components[1] = kInstanceIdKey.data();
    components[2] = kRegionKey.data();
    return std::make_pair(std::cbegin(components), std::cend(components));
  }();
  return iterator_pair;
}

}  // namespace

namespace google::scp::cpio::client_providers {

ExecutionResultOr<std::shared_ptr<EC2Client>>
AwsInstanceClientProvider::GetEC2ClientByRegion(
    std::string_view region) noexcept {
  // Available Regions. Refers to
  // https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/using-regions-availability-zones.html
  // Static duration map is heap allocated to avoid destructor call.
  static const auto& kAwsRegionCodes = *new absl::flat_hash_set<std::string>{
      "us-east-2",      "us-east-1",      "us-west-1",      "us-west-2",
      "af-south-1",     "ap-east-1",      "ap-south-2",     "ap-southeast-3",
      "ap-southeast-4", "ap-south-1",     "ap-northeast-3", "ap-northeast-2",
      "ap-southeast-1", "ap-southeast-2", "ap-northeast-1", "ca-central-1",
      "eu-central-1",   "eu-west-1",      "eu-west-2",      "eu-south-1",
      "eu-west-3",      "eu-south-2",     "eu-north-1",     "eu-central-2",
      "me-south-1",     "me-central-1",   "sa-east-1",
  };
  std::string target_region;
  if (region.empty()) {
    target_region = kDefaultRegionCode;
  } else {
    target_region = std::string{region};
  }
  auto it = kAwsRegionCodes.find(target_region);
  if (it == kAwsRegionCodes.end()) {
    return FailureExecutionResult(SC_AWS_INSTANCE_CLIENT_INVALID_REGION_CODE);
  }

  {
    absl::MutexLock lock(&mu_);
    if (const auto it = ec2_clients_list_.find(target_region);
        it != ec2_clients_list_.end()) {
      return it->second;
    }
  }

  ASSIGN_OR_RETURN(
      std::shared_ptr<EC2Client> ec2_client,
      ec2_factory_.CreateClient(target_region, io_async_executor_));

  absl::MutexLock lock(&mu_);
  ec2_clients_list_[std::move(target_region)] = ec2_client;
  return ec2_client;
}

absl::Status AwsInstanceClientProvider::GetCurrentInstanceResourceNameSync(
    std::string& resource_name) noexcept {
  GetCurrentInstanceResourceNameRequest request;
  GetCurrentInstanceResourceNameResponse response;
  if (absl::Status error =
          CpioUtils::AsyncToSync<GetCurrentInstanceResourceNameRequest,
                                 GetCurrentInstanceResourceNameResponse>(
              absl::bind_front(
                  &AwsInstanceClientProvider::GetCurrentInstanceResourceName,
                  this),
              std::move(request), response);

      !error.ok()) {
    SCP_ERROR(kAwsInstanceClientProvider, kZeroUuid, error,
              "Failed to run async function GetCurrentInstanceResourceName for "
              "current instance resource name");
    return error;
  }

  resource_name = std::move(*response.mutable_instance_resource_name());

  return absl::OkStatus();
}

absl::Status AwsInstanceClientProvider::GetCurrentInstanceResourceName(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>&
        get_resource_name_context) noexcept {
  AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>
      get_token_context(
          std::make_shared<GetSessionTokenRequest>(),
          absl::bind_front(
              &AwsInstanceClientProvider::OnGetSessionTokenCallback, this,
              get_resource_name_context),
          get_resource_name_context);
  if (const ExecutionResult execution_result =
          auth_token_provider_->GetSessionToken(get_token_context);
      !execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kAwsInstanceClientProvider, get_resource_name_context,
                      execution_result,
                      "Failed to get the session token for current instance.");
    get_resource_name_context.Finish(execution_result);

    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }

  return absl::OkStatus();
}

void AwsInstanceClientProvider::OnGetSessionTokenCallback(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>&
        get_resource_name_context,
    AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context) noexcept {
  if (!get_token_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kAwsInstanceClientProvider, get_resource_name_context,
                      get_token_context.result,
                      "Failed to get the access token.");
    get_resource_name_context.Finish(get_token_context.result);
    return;
  }

  auto signed_request = std::make_shared<HttpRequest>();
  signed_request->path =
      std::make_shared<std::string>(kAwsInstanceDynamicDataUrl);
  signed_request->method = HttpMethod::GET;

  const auto& access_token = *get_token_context.response->session_token;
  signed_request->headers = std::make_shared<core::HttpHeaders>();
  signed_request->headers->insert(
      {std::string(kAuthorizationHeaderKey), access_token});

  AsyncContext<HttpRequest, HttpResponse> http_context(
      std::move(signed_request),
      absl::bind_front(
          &AwsInstanceClientProvider::OnGetInstanceResourceNameCallback, this,
          get_resource_name_context),
      get_resource_name_context);

  auto execution_result = http1_client_->PerformRequest(http_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kAwsInstanceClientProvider, get_resource_name_context, execution_result,
        "Failed to perform http request to get the current instance "
        "resource id.");
    get_resource_name_context.Finish(execution_result);
    return;
  }
}

void AwsInstanceClientProvider::OnGetInstanceResourceNameCallback(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>&
        get_resource_name_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context) noexcept {
  if (!http_client_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kAwsInstanceClientProvider, get_resource_name_context,
                      http_client_context.result,
                      "Failed to get the current instance resource id.");
    get_resource_name_context.Finish(http_client_context.result);
    return;
  }

  auto malformed_failure = FailureExecutionResult(
      SC_AWS_INSTANCE_CLIENT_INSTANCE_RESOURCE_NAME_RESPONSE_MALFORMED);

  json json_response;
  try {
    json_response =
        json::parse(http_client_context.response->body.bytes->begin(),
                    http_client_context.response->body.bytes->end());
  } catch (...) {
    SCP_ERROR_CONTEXT(
        kAwsInstanceClientProvider, get_resource_name_context,
        malformed_failure,
        "Received http response could not be parsed into a JSON.");
    get_resource_name_context.Finish(malformed_failure);
    return;
  }

  if (!std::all_of(GetRequiredFieldsForInstanceDynamicData().first,
                   GetRequiredFieldsForInstanceDynamicData().second,
                   [&json_response](const char* const component) {
                     return json_response.contains(component);
                   })) {
    SCP_ERROR_CONTEXT(
        kAwsInstanceClientProvider, get_resource_name_context,
        malformed_failure,
        "Received http response doesn't contain the required fields.");
    get_resource_name_context.Finish(malformed_failure);
    return;
  }

  auto resource_name =
      absl::Substitute(kAwsInstanceResourceNameFormat,
                       json_response[kRegionKey].get<std::string>(),
                       json_response[kAccountIdKey].get<std::string>(),
                       json_response[kInstanceIdKey].get<std::string>());

  get_resource_name_context.response =
      std::make_shared<GetCurrentInstanceResourceNameResponse>();
  get_resource_name_context.response->set_instance_resource_name(resource_name);
  get_resource_name_context.Finish(SuccessExecutionResult());
}

absl::Status AwsInstanceClientProvider::GetInstanceDetailsByResourceNameSync(
    std::string_view resource_name,
    InstanceDetails& instance_details) noexcept {
  GetInstanceDetailsByResourceNameRequest request;
  request.set_instance_resource_name(resource_name);
  GetInstanceDetailsByResourceNameResponse response;
  if (absl::Status error =
          CpioUtils::AsyncToSync<GetInstanceDetailsByResourceNameRequest,
                                 GetInstanceDetailsByResourceNameResponse>(
              absl::bind_front(
                  &AwsInstanceClientProvider::GetInstanceDetailsByResourceName,
                  this),
              request, response);
      !error.ok()) {
    SCP_ERROR(kAwsInstanceClientProvider, kZeroUuid, error,
              "Failed to run async function GetInstanceDetailsByResourceName "
              "for resource %s",
              request.instance_resource_name().c_str());
    return error;
  }

  instance_details = std::move(*response.mutable_instance_details());

  return absl::OkStatus();
}

absl::Status AwsInstanceClientProvider::GetInstanceDetailsByResourceName(
    AsyncContext<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>&
        get_details_context) noexcept {
  AwsResourceNameDetails details;
  if (const ExecutionResult execution_result =
          AwsInstanceClientUtils::GetResourceNameDetails(
              get_details_context.request->instance_resource_name(), details);
      !execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kAwsInstanceClientProvider, get_details_context, execution_result,
        "Get tags request failed due to invalid resource name %s",
        get_details_context.request->instance_resource_name().c_str());
    get_details_context.Finish(execution_result);
    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }

  auto ec2_client_or = GetEC2ClientByRegion(details.region);
  if (!ec2_client_or.Successful()) {
    SCP_ERROR_CONTEXT(
        kAwsInstanceClientProvider, get_details_context, ec2_client_or.result(),
        "Get tags request failed to create EC2Client for resource name %s",
        get_details_context.request->instance_resource_name().c_str());
    get_details_context.Finish(ec2_client_or.result());
    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        ec2_client_or.result().status_code));
  }

  DescribeInstancesRequest request;
  request.AddInstanceIds(details.resource_id);

  (*ec2_client_or)
      ->DescribeInstancesAsync(
          request,
          absl::bind_front(
              &AwsInstanceClientProvider::OnDescribeInstancesAsyncCallback,
              this, get_details_context));

  return absl::OkStatus();
}

void AwsInstanceClientProvider::OnDescribeInstancesAsyncCallback(
    AsyncContext<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>& get_details_context,
    const EC2Client*, const DescribeInstancesRequest&,
    const DescribeInstancesOutcome& outcome,
    const std::shared_ptr<const AsyncCallerContext>&) noexcept {
  if (!outcome.IsSuccess()) {
    const auto& error_type = outcome.GetError().GetErrorType();
    auto result = EC2ErrorConverter::ConvertEC2Error(
        error_type, outcome.GetError().GetMessage());
    SCP_ERROR_CONTEXT(
        kAwsInstanceClientProvider, get_details_context, result,
        "Describe instances request failed for instance %s",
        get_details_context.request->instance_resource_name().c_str());
    FinishContext(result, get_details_context, *cpu_async_executor_);
    return;
  }

  if (outcome.GetResult().GetReservations().size() != 1 ||
      outcome.GetResult().GetReservations()[0].GetInstances().size() != 1) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_INSTANCE_CLIENT_PROVIDER_DESCRIBE_INSTANCES_RESPONSE_MALFORMED);
    SCP_ERROR_CONTEXT(
        kAwsInstanceClientProvider, get_details_context, execution_result,
        "Describe instances response data size doesn't match with "
        "one instance request for instance %s",
        get_details_context.request->instance_resource_name().c_str());

    FinishContext(execution_result, get_details_context, *cpu_async_executor_);
    return;
  }

  const auto& target_instance =
      outcome.GetResult().GetReservations()[0].GetInstances()[0];

  get_details_context.response =
      std::make_shared<GetInstanceDetailsByResourceNameResponse>();
  auto* instance_details =
      get_details_context.response->mutable_instance_details();

  instance_details->set_instance_id(target_instance.GetInstanceId().c_str());

  auto* network = instance_details->add_networks();
  network->set_private_ipv4_address(
      target_instance.GetPrivateIpAddress().c_str());
  network->set_public_ipv4_address(
      target_instance.GetPublicIpAddress().c_str());

  // Extract instance labels.
  auto& labels_proto = *instance_details->mutable_labels();
  for (const auto& tag : target_instance.GetTags()) {
    labels_proto[tag.GetKey()] = tag.GetValue();
  }

  FinishContext(SuccessExecutionResult(), get_details_context,
                *cpu_async_executor_);
}

absl::Status AwsInstanceClientProvider::GetTagsByResourceName(
    AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>&
        get_tags_context) noexcept {
  AwsResourceNameDetails details;
  if (const ExecutionResult execution_result =
          AwsInstanceClientUtils::GetResourceNameDetails(
              get_tags_context.request->resource_name(), details);
      !execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kAwsInstanceClientProvider, get_tags_context,
                      execution_result,
                      "Get tags request failed due to invalid resource name %s",
                      get_tags_context.request->resource_name().c_str());
    get_tags_context.Finish(execution_result);
    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }

  auto ec2_client_or = GetEC2ClientByRegion(details.region);
  if (!ec2_client_or.Successful()) {
    SCP_ERROR_CONTEXT(
        kAwsInstanceClientProvider, get_tags_context, ec2_client_or.result(),
        "Get tags request failed to create EC2Client for resource name %s",
        get_tags_context.request->resource_name().c_str());
    get_tags_context.Finish(ec2_client_or.result());
    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        ec2_client_or.result().status_code));
  }

  DescribeTagsRequest request;

  Filter resource_name_filter;
  resource_name_filter.SetName(std::string{kResourceIdFilterName});
  resource_name_filter.AddValues(details.resource_id);
  request.AddFilters(resource_name_filter);

  (*ec2_client_or)
      ->DescribeTagsAsync(
          request, absl::bind_front(
                       &AwsInstanceClientProvider::OnDescribeTagsAsyncCallback,
                       this, get_tags_context));
  return absl::OkStatus();
}

void AwsInstanceClientProvider::OnDescribeTagsAsyncCallback(
    AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>&
        get_tags_context,
    const EC2Client*, const DescribeTagsRequest&,
    const DescribeTagsOutcome& outcome,
    const std::shared_ptr<const AsyncCallerContext>&) noexcept {
  if (!outcome.IsSuccess()) {
    const auto& error_type = outcome.GetError().GetErrorType();
    auto result = EC2ErrorConverter::ConvertEC2Error(
        error_type, outcome.GetError().GetMessage());
    SCP_ERROR_CONTEXT(kAwsInstanceClientProvider, get_tags_context, result,
                      "Get tags request failed for resource %s",
                      get_tags_context.request->resource_name().c_str());
    FinishContext(result, get_tags_context, *cpu_async_executor_);
    return;
  }

  get_tags_context.response = std::make_shared<GetTagsByResourceNameResponse>();
  auto& tags = *get_tags_context.response->mutable_tags();

  for (const auto& tag : outcome.GetResult().GetTags()) {
    tags[tag.GetKey()] = tag.GetValue();
  }

  FinishContext(SuccessExecutionResult(), get_tags_context,
                *cpu_async_executor_);
}

absl::Status AwsInstanceClientProvider::ListInstanceDetailsByEnvironment(
    AsyncContext<ListInstanceDetailsByEnvironmentRequest,
                 ListInstanceDetailsByEnvironmentResponse>&
        get_instance_details_context) noexcept {
  auto result = FailureExecutionResult(
      core::errors::SC_AWS_INSTANCE_CLIENT_NOT_IMPLEMENTED);
  SCP_ERROR_CONTEXT(kAwsInstanceClientProvider, get_instance_details_context,
                    result, "Not implemented");
  get_instance_details_context.Finish(result);
  return absl::UnimplementedError(
      google::scp::core::errors::GetErrorMessage(result.status_code));
}

ExecutionResultOr<std::unique_ptr<EC2Client>> AwsEC2ClientFactory::CreateClient(
    std::string_view region,
    AsyncExecutorInterface* io_async_executor) noexcept {
  auto client_config = common::CreateClientConfiguration(std::string(region));
  client_config.maxConnections = kMaxConcurrentConnections;
  client_config.executor =
      std::make_shared<AwsAsyncExecutor>(io_async_executor);
  return std::make_unique<EC2Client>(std::move(client_config));
}

absl::Nonnull<std::unique_ptr<InstanceClientProviderInterface>>
InstanceClientProviderFactory::Create(
    absl::Nonnull<AuthTokenProviderInterface*> auth_token_provider,
    absl::Nonnull<HttpClientInterface*> http1_client,
    absl::Nonnull<HttpClientInterface*> /*http2_client*/,
    absl::Nonnull<AsyncExecutorInterface*> cpu_async_executor,
    absl::Nonnull<AsyncExecutorInterface*> io_async_executor) {
  return std::make_unique<AwsInstanceClientProvider>(
      auth_token_provider, http1_client, cpu_async_executor, io_async_executor);
}
}  // namespace google::scp::cpio::client_providers
