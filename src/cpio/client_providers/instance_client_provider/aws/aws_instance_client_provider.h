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

#ifndef CPIO_CLIENT_PROVIDERS_INSTANCE_CLIENT_PROVIDER_AWS_AWS_INSTANCE_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_INSTANCE_CLIENT_PROVIDER_AWS_AWS_INSTANCE_CLIENT_PROVIDER_H_

#include <memory>
#include <string>
#include <utility>

#include <aws/ec2/EC2Client.h>

#include "absl/base/nullability.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "src/public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {

/// Creates Aws::EC2::EC2Client
class AwsEC2ClientFactory {
 public:
  virtual core::ExecutionResultOr<std::unique_ptr<Aws::EC2::EC2Client>>
  CreateClient(std::string_view region,
               core::AsyncExecutorInterface* io_async_executor) noexcept;

  virtual ~AwsEC2ClientFactory() = default;
};

/*! @copydoc InstanceClientProviderInterface
 */
class AwsInstanceClientProvider : public InstanceClientProviderInterface {
 public:
  /// Constructs a new Aws Instance Client Provider object
  AwsInstanceClientProvider(
      absl::Nonnull<AuthTokenProviderInterface*> auth_token_provider,
      absl::Nonnull<core::HttpClientInterface*> http1_client,
      absl::Nonnull<core::AsyncExecutorInterface*> cpu_async_executor,
      absl::Nonnull<core::AsyncExecutorInterface*> io_async_executor,
      AwsEC2ClientFactory ec2_factory = AwsEC2ClientFactory())
      : auth_token_provider_(auth_token_provider),
        http1_client_(http1_client),
        cpu_async_executor_(cpu_async_executor),
        io_async_executor_(io_async_executor),
        ec2_factory_(std::move(ec2_factory)) {}

  absl::Status GetCurrentInstanceResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameResponse>&
          context) noexcept override;

  absl::Status GetTagsByResourceName(
      core::AsyncContext<
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest,
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse>&
          context) noexcept override;

  absl::Status GetInstanceDetailsByResourceName(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameResponse>&
          context) noexcept override;

  // Not implemented. AWS SDK supports this command, see
  // `DescribeAutoScalingGroups`. Please use it directly. Alternatively, that
  // SDK logic can also be put here.
  absl::Status ListInstanceDetailsByEnvironment(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             ListInstanceDetailsByEnvironmentRequest,
                         cmrt::sdk::instance_service::v1::
                             ListInstanceDetailsByEnvironmentResponse>&
          context) noexcept override;

  absl::Status GetCurrentInstanceResourceNameSync(
      std::string& resource_name) noexcept override;

  absl::Status GetInstanceDetailsByResourceNameSync(
      std::string_view resource_name,
      cmrt::sdk::instance_service::v1::InstanceDetails&
          instance_details) noexcept override;

 private:
  /**
   * @brief Is called after auth_token_provider GetSessionToken() for session
   * token is completed
   *
   * @param get_resource_name_context the context for getting current instance
   * resource id.
   * @param get_token_context the context of get session token.
   */
  void OnGetSessionTokenCallback(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameResponse>&
          get_resource_name_context,
      core::AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
          get_token_context) noexcept;

  /**
   * @brief Is called after http client PerformRequest() for current instance
   * id is completed.
   *
   * @param get_resource_name_context the context for getting current instance
   * resource id.
   * @param http_client_context the context for http_client PerformRequest().
   */
  void OnGetInstanceResourceNameCallback(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetCurrentInstanceResourceNameResponse>&
          get_resource_name_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_client_context) noexcept;

  /**
   * @brief Is called after ec2_client DescribeInstancesAsync() for a given
   * instance is completed.
   *
   * @param get_details_context the context to get the details of a instance.
   * @param outcome the outcome of DescribeInstancesAsync().
   */
  void OnDescribeInstancesAsyncCallback(
      core::AsyncContext<cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameRequest,
                         cmrt::sdk::instance_service::v1::
                             GetInstanceDetailsByResourceNameResponse>&
          get_details_context,
      const Aws::EC2::EC2Client*,
      const Aws::EC2::Model::DescribeInstancesRequest&,
      const Aws::EC2::Model::DescribeInstancesOutcome& outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) noexcept;

  /**
   * @brief Is called after ec2_client DescribeTagsAsync() for a given
   * resource id is completed.
   *
   * @param get_tags_context the context to get the tags of a given resource id.
   * @param outcome the outcome of DescribeTagsAsync().
   */
  void OnDescribeTagsAsyncCallback(
      core::AsyncContext<
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest,
          cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse>&
          get_tags_context,
      const Aws::EC2::EC2Client*, const Aws::EC2::Model::DescribeTagsRequest&,
      const Aws::EC2::Model::DescribeTagsOutcome& outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) noexcept;

  /**
   * @brief Get EC2 clients from ec2_clients_list_ by region code. If this EC2
   * client does not exist, create a new one and store it in ec2_clients_list_.
   *
   * @param region AWS region code. If the region is an empty string, the
   * default region code `us-east-1` will be applied.
   * @return core::ExecutionResultOr<std::shared_ptr<Aws::EC2::EC2Client>> EC2
   * client if success.
   */
  core::ExecutionResultOr<std::shared_ptr<Aws::EC2::EC2Client>>
  GetEC2ClientByRegion(std::string_view region) noexcept
      ABSL_LOCKS_EXCLUDED(mu_);

  /// On-demand EC2 client for region codes.
  absl::flat_hash_map<std::string, std::shared_ptr<Aws::EC2::EC2Client>>
      ec2_clients_list_ ABSL_GUARDED_BY(mu_);
  absl::Mutex mu_;

  /// Instance of auth token provider.
  AuthTokenProviderInterface* auth_token_provider_;
  /// Instance of http client.
  core::HttpClientInterface* http1_client_;
  /// Instances of the async executor for local compute and blocking IO
  /// operations respectively.
  core::AsyncExecutorInterface* cpu_async_executor_;
  core::AsyncExecutorInterface* io_async_executor_;

  /// An instance of the factory for Aws::EC2::EC2Client.
  AwsEC2ClientFactory ec2_factory_;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_INSTANCE_CLIENT_PROVIDER_AWS_AWS_INSTANCE_CLIENT_PROVIDER_H_
