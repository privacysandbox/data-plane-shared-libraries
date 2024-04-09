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

#include "gcp_instance_client_provider.h"

#include <future>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/base/nullability.h"
#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/substitute.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/common/cpio_utils.h"
#include "src/public/core/interface/execution_result.h"

#include "error_codes.h"
#include "gcp_instance_client_utils.h"

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
using google::cmrt::sdk::instance_service::v1::
    ListInstanceDetailsByEnvironmentRequest;
using google::cmrt::sdk::instance_service::v1::
    ListInstanceDetailsByEnvironmentResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_INSTANCE_DETAILS_RESPONSE_MALFORMED;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_TYPE;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_PROVIDER_SERVICE_UNAVAILABLE;
using google::scp::core::errors::
    SC_GCP_INSTANCE_CLIENT_RESOURCE_TAGS_RESPONSE_MALFORMED;
using google::scp::core::errors::SC_GCP_INSTANCE_CLIENT_ZONE_PARSING_FAILURE;
using google::scp::cpio::client_providers::GcpInstanceClientUtils;
using google::scp::cpio::common::CpioUtils;
using nlohmann::json;

namespace {
constexpr std::string_view kGcpInstanceClientProvider =
    "GcpInstanceClientProvider";

constexpr std::string_view kURIForInstancePrivateIpv4Address =
    "http://metadata.google.internal/computeMetadata/v1/instance/"
    "network-interfaces/0/ip";
constexpr std::string_view kURIForInstanceId =
    "http://metadata.google.internal/computeMetadata/v1/instance/id";
constexpr std::string_view kURIForInstanceZone =
    "http://metadata.google.internal/computeMetadata/v1/instance/zone";
constexpr std::string_view kURIForProjectId =
    "http://metadata.google.internal/computeMetadata/v1/project/project-id";
constexpr std::string_view kMetadataFlavorHeaderKey = "Metadata-Flavor";
constexpr std::string_view kMetadataFlavorHeaderValue = "Google";

constexpr std::string_view kGcpInstanceRNFormatString =
    "//compute.googleapis.com/projects/$0/zones/$1/instances/$2";

constexpr std::string_view kInstanceResourceNamePrefix =
    "//compute.googleapis.com/";
constexpr std::string_view kGcpInstanceGetUrlPrefix =
    "https://compute.googleapis.com/compute/v1/";
constexpr std::string_view kListInstancesFormatString =
    "https://compute.googleapis.com/compute/v1/projects/$0/aggregated/"
    "instances?filter=(labels.environment=$1)";
constexpr std::string_view kQueryWithPageTokenFormatString = "&pageToken=$0";
constexpr std::string_view kAuthorizationHeaderKey = "Authorization";
constexpr std::string_view kBearerTokenPrefix = "Bearer ";
constexpr std::string_view kInstanceDetailsJsonIdKey = "id";
constexpr std::string_view kNetworkInterfacesKey = "networkInterfaces";
constexpr std::string_view kPrivateIpKey = "networkIP";
constexpr std::string_view kAccessConfigs = "accessConfigs";
constexpr std::string_view kPublicIpKey = "natIP";
constexpr std::string_view kInstanceLabelsKey = "labels";

constexpr std::string_view kParentParameter = "parent=";

// The server allows a maximum of 300 TagBindings to return.
// For more information, see
// https://cloud.google.com/resource-manager/reference/rest/v3/tagBindings/list
constexpr std::string_view kPageSizeSetting = "pageSize=300";
constexpr std::string_view kTagBindingNameKey = "name";
constexpr std::string_view kTagBindingParentKey = "parent";
constexpr std::string_view kTagBindingTagValueKey = "tagValue";
constexpr std::string_view kTagBindingsListKey = "tagBindings";
constexpr std::string_view kItmes = "items";
constexpr std::string_view kInstances = "instances";
constexpr std::string_view kNextPageToken = "nextPageToken";

// Returns a pair of iterators - one to the beginning, one to the end.
const auto& GetRequiredFieldsForInstanceDetails() {
  static char const* components[2];
  using iterator_type = decltype(std::cbegin(components));
  static std::pair<iterator_type, iterator_type> iterator_pair = []() {
    components[0] = kInstanceDetailsJsonIdKey.data();
    components[1] = kNetworkInterfacesKey.data();
    return std::make_pair(std::cbegin(components), std::cend(components));
  }();
  return iterator_pair;
}

const auto& GetRequiredFieldsForResourceTags() {
  static char const* components[3];
  using iterator_type = decltype(std::cbegin(components));
  static std::pair<iterator_type, iterator_type> iterator_pair = []() {
    components[0] = kTagBindingNameKey.data();
    components[1] = kTagBindingParentKey.data();
    components[2] = kTagBindingTagValueKey.data();
    return std::make_pair(std::cbegin(components), std::cend(components));
  }();
  return iterator_pair;
}

}  // namespace

namespace google::scp::cpio::client_providers {

GcpInstanceClientProvider::GcpInstanceClientProvider(
    absl::Nonnull<AuthTokenProviderInterface*> auth_token_provider,
    absl::Nonnull<HttpClientInterface*> http1_client,
    absl::Nonnull<HttpClientInterface*> http2_client)
    : http1_client_(http1_client),
      http2_client_(http2_client),
      auth_token_provider_(auth_token_provider),
      http_uri_instance_private_ipv4_(
          std::make_shared<std::string>(kURIForInstancePrivateIpv4Address)),
      http_uri_instance_id_(std::make_shared<std::string>(kURIForInstanceId)),
      http_uri_project_id_(std::make_shared<std::string>(kURIForProjectId)),
      http_uri_instance_zone_(
          std::make_shared<std::string>(kURIForInstanceZone)) {}

absl::Status GcpInstanceClientProvider::GetCurrentInstanceResourceNameSync(
    std::string& resource_name) noexcept {
  GetCurrentInstanceResourceNameRequest request;
  GetCurrentInstanceResourceNameResponse response;
  if (absl::Status error =
          CpioUtils::AsyncToSync<GetCurrentInstanceResourceNameRequest,
                                 GetCurrentInstanceResourceNameResponse>(
              absl::bind_front(
                  &GcpInstanceClientProvider::GetCurrentInstanceResourceName,
                  this),
              std::move(request), response);
      !error.ok()) {
    SCP_ERROR(kGcpInstanceClientProvider, kZeroUuid, error,
              "Failed to run async function GetCurrentInstanceResourceName for "
              "current instance resource name");
    return error;
  }

  resource_name = std::move(*response.mutable_instance_resource_name());

  return absl::OkStatus();
}

absl::Status GcpInstanceClientProvider::GetCurrentInstanceResourceName(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>&
        get_resource_name_context) noexcept {
  auto instance_resource_name_tracker =
      std::make_shared<InstanceResourceNameTracker>();

  if (ExecutionResult execution_result =
          MakeHttpRequestsForInstanceResourceName(
              get_resource_name_context, http_uri_project_id_,
              instance_resource_name_tracker, ResourceType::kProjectId);
      !execution_result.Successful()) {
    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }

  if (ExecutionResult execution_result =
          MakeHttpRequestsForInstanceResourceName(
              get_resource_name_context, http_uri_instance_zone_,
              instance_resource_name_tracker, ResourceType::kZone);
      !execution_result.Successful()) {
    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }

  if (ExecutionResult execution_result =
          MakeHttpRequestsForInstanceResourceName(
              get_resource_name_context, http_uri_instance_id_,
              instance_resource_name_tracker, ResourceType::kInstanceId);
      !execution_result.Successful()) {
    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }

  return absl::OkStatus();
}

ExecutionResult
GcpInstanceClientProvider::MakeHttpRequestsForInstanceResourceName(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>&
        get_resource_name_context,
    std::shared_ptr<std::string>& uri,
    std::shared_ptr<InstanceResourceNameTracker> instance_resource_name_tracker,
    ResourceType type) noexcept {
  auto http_request = std::make_shared<HttpRequest>();
  http_request->method = HttpMethod::GET;
  http_request->path = uri;
  http_request->body.length = 0;
  http_request->headers = std::make_shared<core::HttpHeaders>();
  http_request->headers->insert({std::string(kMetadataFlavorHeaderKey),
                                 std::string(kMetadataFlavorHeaderValue)});

  AsyncContext<HttpRequest, HttpResponse> http_context(
      std::move(http_request),
      std::bind(&GcpInstanceClientProvider::OnGetInstanceResourceName, this,
                get_resource_name_context, std::placeholders::_1,
                instance_resource_name_tracker, type),
      get_resource_name_context);

  auto execution_result = http1_client_->PerformRequest(http_context);
  if (!execution_result.Successful()) {
    // If got_failure is false, then the other thread hasn't failed - we should
    // be the ones to log and finish the context.
    if (absl::MutexLock lock(&instance_resource_name_tracker->got_failure_mu);
        !instance_resource_name_tracker->got_failure) {
      instance_resource_name_tracker->got_failure = true;
      SCP_ERROR_CONTEXT(
          kGcpInstanceClientProvider, get_resource_name_context,
          execution_result,
          "Failed to perform http request to fetch resource id with uri %s",
          uri->c_str());
      get_resource_name_context.Finish(execution_result);
    }
    return execution_result;
  }

  return SuccessExecutionResult();
}

void GcpInstanceClientProvider::OnGetInstanceResourceName(
    AsyncContext<GetCurrentInstanceResourceNameRequest,
                 GetCurrentInstanceResourceNameResponse>&
        get_resource_name_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context,
    std::shared_ptr<InstanceResourceNameTracker> instance_resource_name_tracker,
    ResourceType type) noexcept {
  // If got_failure is true, no need to process this request.
  if (absl::MutexLock lock(&instance_resource_name_tracker->got_failure_mu);
      instance_resource_name_tracker->got_failure) {
    return;
  }

  auto result = http_client_context.result;
  if (!result.Successful()) {
    // If got_failure is false, then the other thread hasn't failed - we should
    // be the ones to log and finish the context.
    if (absl::MutexLock lock(&instance_resource_name_tracker->got_failure_mu);
        !instance_resource_name_tracker->got_failure) {
      instance_resource_name_tracker->got_failure = true;
      SCP_ERROR_CONTEXT(
          kGcpInstanceClientProvider, get_resource_name_context, result,
          "Failed to perform http request to fetch resource id with uri %s",
          http_client_context.request->path->c_str());
      get_resource_name_context.Finish(result);
    }
    return;
  }

  switch (type) {
    case ResourceType::kProjectId: {
      instance_resource_name_tracker->project_id =
          http_client_context.response->body.ToString();
      break;
    }
    case ResourceType::kInstanceId: {
      instance_resource_name_tracker->instance_id =
          http_client_context.response->body.ToString();
      break;
    }
    case ResourceType::kZone: {
      // The response from zone fetching is
      // `projects/PROJECT_NUMBER/zones/ZONE_ID`, and the project number is
      // different from project ID. In some cases, the project number doesn't
      // work. (e.g, in the spanner, using project number will have permission
      // issue)
      std::vector<std::string> splits =
          absl::StrSplit(http_client_context.response->body.ToString(), "/");
      instance_resource_name_tracker->instance_zone = std::move(splits.back());
      break;
    }
    default: {
      auto execution_result = FailureExecutionResult(
          SC_GCP_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_TYPE);
      SCP_ERROR_CONTEXT(kGcpInstanceClientProvider, get_resource_name_context,
                        execution_result, "Invalid instance resource type %d.",
                        type);
      get_resource_name_context.Finish(execution_result);
    }
  }

  int num_outstanding_calls;
  {
    absl::MutexLock lock(
        &instance_resource_name_tracker->num_outstanding_calls_mu);
    num_outstanding_calls =
        --instance_resource_name_tracker->num_outstanding_calls;
  }
  if (num_outstanding_calls == 0) {
    get_resource_name_context.response =
        std::make_shared<GetCurrentInstanceResourceNameResponse>();
    // The instance resource name is
    // `projects/PROJECT_ID/zones/ZONE_ID/instances/INSTANCE_ID`.
    auto resource_name = absl::Substitute(
        kGcpInstanceRNFormatString, instance_resource_name_tracker->project_id,
        instance_resource_name_tracker->instance_zone,
        instance_resource_name_tracker->instance_id);
    get_resource_name_context.response->set_instance_resource_name(
        resource_name);
    get_resource_name_context.Finish(SuccessExecutionResult());
  }
}

absl::Status GcpInstanceClientProvider::GetTagsByResourceName(
    AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>&
        get_tags_context) noexcept {
  AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>
      get_token_context(
          std::make_shared<GetSessionTokenRequest>(),
          absl::bind_front(
              &GcpInstanceClientProvider::OnGetSessionTokenForTagsCallback,
              this, get_tags_context),
          get_tags_context);

  if (ExecutionResult execution_result =
          auth_token_provider_->GetSessionToken(get_token_context);
      !execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpInstanceClientProvider, get_tags_context,
                      execution_result,
                      "Failed to get the tags for resource %s",
                      get_tags_context.request->resource_name().c_str());
    get_tags_context.Finish(execution_result);

    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }

  return absl::OkStatus();
}

void GcpInstanceClientProvider::OnGetSessionTokenForTagsCallback(
    AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>&
        get_tags_context,
    AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context) noexcept {
  if (!get_token_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpInstanceClientProvider, get_tags_context,
                      get_token_context.result,
                      "Failed to get the access token for resource %s",
                      get_tags_context.request->resource_name().c_str());
    get_tags_context.Finish(get_token_context.result);
    return;
  }

  auto uri = GcpInstanceClientUtils::CreateRMListTagsUrl(
      get_tags_context.request->resource_name());
  auto signed_request = std::make_shared<HttpRequest>();
  signed_request->path = std::make_shared<std::string>(std::move(uri));
  signed_request->method = HttpMethod::GET;
  signed_request->query = std::make_shared<std::string>(absl::StrCat(
      kParentParameter, get_tags_context.request->resource_name().c_str(), "&",
      kPageSizeSetting));

  const auto& access_token = *get_token_context.response->session_token;
  signed_request->headers = std::make_shared<core::HttpHeaders>();
  signed_request->headers->insert(
      {std::string(kAuthorizationHeaderKey),
       absl::StrCat(kBearerTokenPrefix, access_token)});

  AsyncContext<HttpRequest, HttpResponse> http_context(
      std::move(signed_request),
      absl::bind_front(
          &GcpInstanceClientProvider::OnGetTagsByResourceNameCallback, this,
          get_tags_context),
      get_tags_context);

  auto execution_result = http2_client_->PerformRequest(http_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpInstanceClientProvider, get_tags_context,
                      execution_result,
                      "Failed to perform http request to get the tags "
                      "of resource %s",
                      get_tags_context.request->resource_name().c_str());
    get_tags_context.Finish(execution_result);
    return;
  }
}

void GcpInstanceClientProvider::OnGetTagsByResourceNameCallback(
    AsyncContext<GetTagsByResourceNameRequest, GetTagsByResourceNameResponse>&
        get_tags_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context) noexcept {
  if (!http_client_context.result.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_tags_context,
        http_client_context.result,
        "Failed to perform http request to get the tags for resource %s",
        get_tags_context.request->resource_name().c_str());
    get_tags_context.Finish(http_client_context.result);
    return;
  }

  auto malformed_failure = FailureExecutionResult(
      SC_GCP_INSTANCE_CLIENT_RESOURCE_TAGS_RESPONSE_MALFORMED);

  json json_response;
  try {
    json_response =
        json::parse(http_client_context.response->body.bytes->begin(),
                    http_client_context.response->body.bytes->end());
  } catch (...) {
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_tags_context, malformed_failure,
        "Received http response could not be parsed into a JSON for "
        "resource %s",
        get_tags_context.request->resource_name().c_str());
    get_tags_context.Finish(malformed_failure);
    return;
  }

  // If json_response is empty, return success with empty response to avoid
  // falling into below malformed checks.
  if (json_response.empty()) {
    get_tags_context.response =
        std::make_shared<GetTagsByResourceNameResponse>();
    get_tags_context.Finish(SuccessExecutionResult());
    return;
  }

  if (!json_response.contains(kTagBindingsListKey)) {
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_tags_context, malformed_failure,
        "Received http response doesn't contain the required fields "
        "for resource %s",
        get_tags_context.request->resource_name().c_str());
    get_tags_context.Finish(malformed_failure);
    return;
  }

  for (const auto& tag_binding : json_response[kTagBindingsListKey]) {
    if (!std::all_of(GetRequiredFieldsForResourceTags().first,
                     GetRequiredFieldsForResourceTags().second,
                     [&tag_binding](const char* const component) {
                       return tag_binding.contains(component);
                     })) {
      SCP_ERROR_CONTEXT(kGcpInstanceClientProvider, get_tags_context,
                        malformed_failure,
                        "Received http response doesn't contain the required "
                        "fields for resource %s",
                        get_tags_context.request->resource_name().c_str());
      get_tags_context.Finish(malformed_failure);
      return;
    }
  }

  get_tags_context.response = std::make_shared<GetTagsByResourceNameResponse>();
  auto& tags = *get_tags_context.response->mutable_tags();
  for (const auto& tag : json_response[kTagBindingsListKey]) {
    tags[tag[kTagBindingNameKey].get<std::string>()] =
        tag[kTagBindingTagValueKey].get<std::string>();
  }
  get_tags_context.Finish(SuccessExecutionResult());
}

absl::Status GcpInstanceClientProvider::GetInstanceDetailsByResourceNameSync(
    std::string_view resource_name,
    cmrt::sdk::instance_service::v1::InstanceDetails&
        instance_details) noexcept {
  GetInstanceDetailsByResourceNameRequest request;
  request.set_instance_resource_name(resource_name);
  GetInstanceDetailsByResourceNameResponse response;
  if (absl::Status error =
          CpioUtils::AsyncToSync<GetInstanceDetailsByResourceNameRequest,
                                 GetInstanceDetailsByResourceNameResponse>(
              absl::bind_front(
                  &GcpInstanceClientProvider::GetInstanceDetailsByResourceName,
                  this),
              request, response);
      !error.ok()) {
    SCP_ERROR(
        kGcpInstanceClientProvider, kZeroUuid, error,
        "Failed to run async function GetInstanceDetailsByResourceName for "
        "resource %s",
        request.instance_resource_name().c_str());
    return error;
  }

  instance_details = std::move(*response.mutable_instance_details());

  return absl::OkStatus();
}

absl::Status GcpInstanceClientProvider::GetInstanceDetailsByResourceName(
    AsyncContext<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>&
        get_instance_details_context) noexcept {
  if (const ExecutionResult execution_result =
          GcpInstanceClientUtils::ValidateInstanceResourceNameFormat(
              get_instance_details_context.request->instance_resource_name());
      !execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_instance_details_context,
        execution_result,
        "Failed to parse instance resource ID from instance resource name %s",
        get_instance_details_context.request->instance_resource_name().c_str());
    get_instance_details_context.Finish(execution_result);
    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }

  AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>
      get_token_context(
          std::make_shared<GetSessionTokenRequest>(),
          absl::bind_front(&GcpInstanceClientProvider::
                               OnGetSessionTokenForInstanceDetailsCallback,
                           this, get_instance_details_context),
          get_instance_details_context);

  if (const ExecutionResult execution_result =
          auth_token_provider_->GetSessionToken(get_token_context);
      !execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_instance_details_context,
        execution_result,
        "Failed to perform http request to get the details of instance %s",
        get_instance_details_context.request->instance_resource_name().c_str());
    get_instance_details_context.Finish(execution_result);

    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }

  return absl::OkStatus();
}

void GcpInstanceClientProvider::OnGetSessionTokenForInstanceDetailsCallback(
    AsyncContext<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>&
        get_instance_details_context,
    AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context) noexcept {
  if (!get_token_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpInstanceClientProvider, get_instance_details_context,
                      get_token_context.result,
                      "Failed to get the access token.");
    get_instance_details_context.Finish(get_token_context.result);
    return;
  }

  auto resource_id =
      get_instance_details_context.request->instance_resource_name().substr(
          kInstanceResourceNamePrefix.length());

  auto uri = absl::StrCat(kGcpInstanceGetUrlPrefix, resource_id);
  auto signed_request = std::make_shared<HttpRequest>();
  signed_request->path = std::make_shared<std::string>(std::move(uri));
  signed_request->method = HttpMethod::GET;

  const auto& access_token = *get_token_context.response->session_token;
  signed_request->headers = std::make_shared<core::HttpHeaders>();
  signed_request->headers->insert(
      {std::string(kAuthorizationHeaderKey),
       absl::StrCat(kBearerTokenPrefix, access_token)});

  AsyncContext<HttpRequest, HttpResponse> http_context(
      std::move(signed_request),
      absl::bind_front(&GcpInstanceClientProvider::OnGetInstanceDetailsCallback,
                       this, get_instance_details_context),
      get_instance_details_context);

  auto execution_result = http2_client_->PerformRequest(http_context);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_instance_details_context,
        execution_result,
        "Failed to perform http request to get the details "
        "of instance %s",
        get_instance_details_context.request->instance_resource_name().c_str());
    get_instance_details_context.Finish(execution_result);
    return;
  }
}

template <typename T, typename Z>
absl::StatusOr<InstanceDetails> ParseInstanceDetails(
    json instance, AsyncContext<T, Z>& get_instance_details_context) {
  if (!std::all_of(GetRequiredFieldsForInstanceDetails().first,
                   GetRequiredFieldsForInstanceDetails().second,
                   [&instance](const char* const component) {
                     return instance.contains(component);
                   })) {
    auto error_message =
        "Received http response doesn't contain the required fields.";
    auto result = FailureExecutionResult(
        SC_GCP_INSTANCE_CLIENT_INSTANCE_DETAILS_RESPONSE_MALFORMED);
    SCP_ERROR_CONTEXT(kGcpInstanceClientProvider, get_instance_details_context,
                      result, error_message);
    get_instance_details_context.Finish(result);
    return absl::InternalError(error_message);
  }
  InstanceDetails instance_details;
  instance_details.set_instance_id(
      instance[kInstanceDetailsJsonIdKey].get<std::string>());
  // Get instance networks info from networkInterfaces.
  for (const auto& network_interface : instance[kNetworkInterfacesKey]) {
    std::string private_ip, public_ip;
    if (network_interface.contains(kPrivateIpKey)) {
      private_ip = network_interface[kPrivateIpKey].get<std::string>();
    }
    // Current GCP only supports one accessConfig.
    if (network_interface.contains(kAccessConfigs)) {
      for (const auto& access_config : network_interface[kAccessConfigs]) {
        if (access_config.contains(kPublicIpKey)) {
          public_ip = access_config[kPublicIpKey].get<std::string>();
          break;
        }
      }
    }
    auto* network = instance_details.add_networks();
    network->set_private_ipv4_address(std::move(private_ip));
    network->set_public_ipv4_address(std::move(public_ip));
  }
  // Extract instance labels.
  auto labels = instance.find(kInstanceLabelsKey);
  if (labels != instance.end()) {
    auto& labels_proto = *instance_details.mutable_labels();
    for (json::iterator it = labels->begin(); it != labels->end(); ++it) {
      labels_proto[it.key()] = it.value().get<std::string>();
    }
  }
  return instance_details;
}

void GcpInstanceClientProvider::OnGetInstanceDetailsCallback(
    AsyncContext<GetInstanceDetailsByResourceNameRequest,
                 GetInstanceDetailsByResourceNameResponse>&
        get_instance_details_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context) noexcept {
  if (!http_client_context.result.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_instance_details_context,
        http_client_context.result,
        "Failed to perform http request to get the details "
        "of instance %s",
        get_instance_details_context.request->instance_resource_name().c_str());
    get_instance_details_context.Finish(http_client_context.result);
    return;
  }

  json json_response;
  try {
    json_response =
        json::parse(http_client_context.response->body.bytes->begin(),
                    http_client_context.response->body.bytes->end());
  } catch (...) {
    auto result = FailureExecutionResult(
        SC_GCP_INSTANCE_CLIENT_INSTANCE_DETAILS_RESPONSE_MALFORMED);
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_instance_details_context, result,
        "Received http response could not be parsed into a JSON.");
    get_instance_details_context.Finish(result);
    return;
  }

  get_instance_details_context.response =
      std::make_shared<GetInstanceDetailsByResourceNameResponse>();
  auto maybe_instance =
      ParseInstanceDetails(json_response, get_instance_details_context);
  if (!maybe_instance.ok()) {
    return;
  }
  auto instance = std::move(*maybe_instance);
  *(get_instance_details_context.response->mutable_instance_details()) =
      std::move(instance);
  get_instance_details_context.Finish(SuccessExecutionResult());
}

absl::Status GcpInstanceClientProvider::ListInstanceDetailsByEnvironment(
    AsyncContext<ListInstanceDetailsByEnvironmentRequest,
                 ListInstanceDetailsByEnvironmentResponse>&
        get_instance_details_context) noexcept {
  if (get_instance_details_context.request->environment().empty() ||
      get_instance_details_context.request->project_id().empty()) {
    const ExecutionResult result = FailureExecutionResult(
        core::errors::SC_GCP_INSTANCE_CLIENT_INSTANCE_INVALID_ARGUMENTS);
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_instance_details_context, result,
        "Must set environment -- %s -- and project id -- %s",
        get_instance_details_context.request->environment().c_str(),
        get_instance_details_context.request->project_id().c_str());
    get_instance_details_context.Finish(result);
    return absl::UnknownError(
        google::scp::core::errors::GetErrorMessage(result.status_code));
  }
  AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>
      get_token_context(
          std::make_shared<GetSessionTokenRequest>(),
          absl::bind_front(&GcpInstanceClientProvider::
                               OnGetSessionTokenForListInstanceDetailsCallback,
                           this, get_instance_details_context),
          get_instance_details_context);
  if (const ExecutionResult execution_result =
          auth_token_provider_->GetSessionToken(get_token_context);
      !execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_instance_details_context,
        execution_result,
        "Failed to perform http request to list the instances "
        "for environment %s",
        get_instance_details_context.request->environment().c_str());
    get_instance_details_context.Finish(execution_result);
    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }
  return absl::OkStatus();
}

void GcpInstanceClientProvider::OnGetSessionTokenForListInstanceDetailsCallback(
    AsyncContext<ListInstanceDetailsByEnvironmentRequest,
                 ListInstanceDetailsByEnvironmentResponse>&
        get_instance_details_context,
    AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
        get_token_context) noexcept {
  if (!get_token_context.result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpInstanceClientProvider, get_instance_details_context,
                      get_token_context.result,
                      "Failed to get the access token.");
    get_instance_details_context.Finish(get_token_context.result);
    return;
  }
  auto environment = get_instance_details_context.request->environment();
  auto project_id = get_instance_details_context.request->project_id();
  std::string uri =
      absl::Substitute(kListInstancesFormatString, project_id, environment);
  if (get_instance_details_context.request->page_token().size() > 0) {
    absl::StrAppend(
        &uri,
        absl::Substitute(kQueryWithPageTokenFormatString,
                         get_instance_details_context.request->page_token()));
  }

  auto signed_request = std::make_shared<HttpRequest>();
  signed_request->path = std::make_shared<std::string>(std::move(uri));
  signed_request->method = HttpMethod::GET;
  const auto access_token = *get_token_context.response->session_token;
  signed_request->headers = std::make_shared<core::HttpHeaders>();
  signed_request->headers->insert(
      {std::string(kAuthorizationHeaderKey),
       absl::StrCat(kBearerTokenPrefix, access_token)});
  AsyncContext<HttpRequest, HttpResponse> http_context(
      std::move(signed_request),
      absl::bind_front(
          &GcpInstanceClientProvider::OnListInstanceDetailsCallback, this,
          get_instance_details_context),
      get_instance_details_context);
  auto execution_result = http2_client_->PerformRequest(http_context);
  if (!execution_result.Successful()) {
    get_instance_details_context.result = execution_result;
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_instance_details_context,
        execution_result,
        "Failed to perform http request to list the instances "
        "for environment %s",
        get_instance_details_context.request->environment().c_str());
    get_instance_details_context.Finish(execution_result);
    return;
  }
}

void GcpInstanceClientProvider::OnListInstanceDetailsCallback(
    AsyncContext<ListInstanceDetailsByEnvironmentRequest,
                 ListInstanceDetailsByEnvironmentResponse>&
        get_instance_details_context,
    AsyncContext<HttpRequest, HttpResponse>& http_client_context) noexcept {
  if (!http_client_context.result.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_instance_details_context,
        http_client_context.result,
        "Failed to perform http request to list the instances "
        "for environment %s",
        get_instance_details_context.request->environment().c_str());
    get_instance_details_context.Finish(http_client_context.result);
    return;
  }

  json json_response;
  try {
    json_response =
        json::parse(http_client_context.response->body.bytes->begin(),
                    http_client_context.response->body.bytes->end());
  } catch (...) {
    auto result = FailureExecutionResult(
        SC_GCP_INSTANCE_CLIENT_INSTANCE_DETAILS_RESPONSE_MALFORMED);
    SCP_ERROR_CONTEXT(
        kGcpInstanceClientProvider, get_instance_details_context, result,
        "Received http response could not be parsed into a JSON.");
    get_instance_details_context.Finish(result);
    return;
  }
  get_instance_details_context.response =
      std::make_shared<ListInstanceDetailsByEnvironmentResponse>();
  std::vector<InstanceDetails> instance_details_list;

  for (auto& element : json_response[kItmes]) {
    if (element.contains(kInstances)) {
      for (auto& instance : element[kInstances]) {
        auto maybe_parsed_instance =
            ParseInstanceDetails(instance, get_instance_details_context);
        if (!maybe_parsed_instance.ok()) {
          return;
        }
        instance_details_list.push_back(std::move(*maybe_parsed_instance));
      }
    }
  }
  get_instance_details_context.response->mutable_instance_details()->Assign(
      instance_details_list.begin(), instance_details_list.end());
  if (json_response.contains(kNextPageToken)) {
    *get_instance_details_context.response->mutable_page_token() =
        json_response[kNextPageToken];
  }
  get_instance_details_context.Finish(SuccessExecutionResult());
}

absl::Nonnull<std::unique_ptr<InstanceClientProviderInterface>>
InstanceClientProviderFactory::Create(
    absl::Nonnull<AuthTokenProviderInterface*> auth_token_provider,
    absl::Nonnull<HttpClientInterface*> http1_client,
    absl::Nonnull<HttpClientInterface*> http2_client,
    absl::Nonnull<AsyncExecutorInterface*> /*async_executor*/,
    absl::Nonnull<AsyncExecutorInterface*> /*io_async_executor*/) {
  return std::make_unique<GcpInstanceClientProvider>(
      auth_token_provider, http1_client, http2_client);
}

}  // namespace google::scp::cpio::client_providers
