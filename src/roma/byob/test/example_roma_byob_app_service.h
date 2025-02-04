/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PRIVACY_SANDBOX_SERVER_COMMON_BYOB_GVISOR_H_
#define PRIVACY_SANDBOX_SERVER_COMMON_BYOB_GVISOR_H_

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/synchronization/notification.h"
#include "src/roma/byob/config/config.h"
#include "src/roma/byob/example/example.pb.h"
#include "src/roma/byob/interface/roma_service.h"
#include "src/util/execution_token.h"
#include "src/util/status_macro/status_macros.h"

namespace privacy_sandbox::server_common::byob::example {
/*
 * service: privacy_sandbox.server_common.byob.EchoService
 */
template <typename TMetadata = google::scp::roma::DefaultMetadata>
class ByobEchoService final {
 public:
  using AppService =
      privacy_sandbox::server_common::byob::RomaService<TMetadata>;
  using Config = privacy_sandbox::server_common::byob::Config<TMetadata>;
  using Mode = privacy_sandbox::server_common::byob::Mode;

  static absl::StatusOr<ByobEchoService<TMetadata>> Create(
      Config config, Mode mode = Mode::kModeGvisorSandbox) {
    auto roma_service = std::make_unique<AppService>();
    PS_RETURN_IF_ERROR(roma_service->Init(std::move(config), mode));
    return ByobEchoService<TMetadata>(std::move(roma_service));
  }

  /**
   * @brief Registers a new binary asynchronously from the provided `code_path`.
   *
   * @paragraph If load is successful, registered service can be called on the
   * code using the `code_token` returned by this function.
   *
   * @param code_path path to the binary to be loaded into the sandbox.
   * @param num_workers number of workers in the worker pool.
   * @return absl::StatusOr<std::string> returns the `code_token`.
   */
  absl::StatusOr<std::string> Register(
      std::filesystem::path code_path,
      int num_workers = std::thread::hardware_concurrency()) {
    return roma_service_->LoadBinary(code_path.c_str(), num_workers);
  }

  void Delete(std::string_view code_token) {
    return roma_service_->Delete(code_token);
  }

  void Cancel(google::scp::roma::ExecutionToken token) {
    return roma_service_->Cancel(std::move(token));
  }

  /**
   * @brief Creates workers for binary with logging with logging enabled.
   *
   * @paragraph If load is successful, registered service can be called on the
   * code using the `code_token` returned by this function.
   *
   * @param code_path path to the binary to be loaded into the sandbox.
   * @param num_workers number of prewarmed workers to be created.
   * Currently, zero workers are not supported.
   * @return absl::StatusOr<std::string> returns the `code_token`.
   */
  absl::StatusOr<std::string> RegisterForLogging(
      std::filesystem::path code_path, int num_workers) {
    return roma_service_->LoadBinaryForLogging(std::move(code_path),
                                               num_workers);
  }

  /**
   * @brief Registers a new binary asynchronously from the provided `code_path`.
   *
   * @paragraph If load is successful, registered service can be called on the
   * code using the `code_token` returned by this function.
   *
   * @param no_log_code_token code token corresponding to the already loaded
   * binary.
   * @param num_workers number of prewarmed workers to be created.
   * Currently, zero workers are not supported.
   * @return absl::StatusOr<std::string> returns the `code_token`.
   */
  absl::StatusOr<std::string> RegisterForLogging(std::string no_log_code_token,
                                                 int num_workers) {
    return roma_service_->LoadBinaryForLogging(std::move(no_log_code_token),
                                               num_workers);
  }

  /*
   * @brief Executes Echo referred to by the provided `code_token`
   * asynchronously.
   *
   * Echo
   *
   *
   * @param notification notifies that `response` is available.
   * @param request ::privacy_sandbox::server_common::byob::example::EchoRequest
   * for the binary.
   * @param response populated with the status once execution is completed. If
   * the status is ok, then
   * `::privacy_sandbox::server_common::byob::example::EchoResponse` returned by
   * this function contains the response else the error.
   * @param metadata for execution request. It is a templated type.
   * @param code_token identifier provided by load of the binary to be executed.
   * @return absl::Status
   */
  absl::StatusOr<google::scp::roma::ExecutionToken> Echo(
      absl::Notification& notification,
      ::privacy_sandbox::server_common::byob::example::EchoRequest request,
      absl::StatusOr<std::unique_ptr<
          ::privacy_sandbox::server_common::byob::example::EchoResponse>>&
          response,
      TMetadata metadata = TMetadata(), std::string_view code_token = "") {
    return roma_service_->ProcessRequest(code_token, std::move(request),
                                         std::move(metadata), notification,
                                         response);
  }

  absl::StatusOr<google::scp::roma::ExecutionToken> Echo(
      absl::AnyInvocable<
          void(absl::StatusOr<
               ::privacy_sandbox::server_common::byob::example::EchoResponse>)>
          callback,
      ::privacy_sandbox::server_common::byob::example::EchoRequest request,
      TMetadata metadata = TMetadata(), std::string_view code_token = "") {
    return roma_service_->template ProcessRequest<
        ::privacy_sandbox::server_common::byob::example::EchoResponse>(
        code_token, std::move(request), std::move(metadata),
        std::move(callback));
  }

  absl::StatusOr<google::scp::roma::ExecutionToken> Echo(
      absl::AnyInvocable<void(absl::StatusOr<::privacy_sandbox::server_common::
                                                 byob::example::EchoResponse>,
                              absl::StatusOr<std::string_view> logs)>
          callback_with_logs_param,
      ::privacy_sandbox::server_common::byob::example::EchoRequest request,
      TMetadata metadata = TMetadata(), std::string_view code_token = "") {
    return roma_service_->template ProcessRequest<
        ::privacy_sandbox::server_common::byob::example::EchoResponse>(
        code_token, std::move(request), std::move(metadata),
        std::move(callback_with_logs_param));
  }

 private:
  std::unique_ptr<AppService> roma_service_;
  explicit ByobEchoService(std::unique_ptr<AppService> roma_service)
      : roma_service_(std::move(roma_service)) {}
};
}  // namespace privacy_sandbox::server_common::byob::example

#endif  // PRIVACY_SANDBOX_SERVER_COMMON_BYOB_GVISOR_H_
