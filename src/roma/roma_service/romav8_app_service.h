/*
 * Copyright 2023 Google LLC
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

#ifndef ROMA_ROMA_SERVICE_ROMAV8_APP_SERVICE_H_
#define ROMA_ROMA_SERVICE_ROMAV8_APP_SERVICE_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "src/roma/config/config.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"
#include "src/roma/roma_service/romav8_proto_utils.h"
#include "src/util/execution_token.h"
#include "src/util/status_macro/status_macros.h"

namespace google::scp::roma::romav8::app_api {

using google::scp::roma::ExecutionToken;

template <typename TMetadata = google::scp::roma::DefaultMetadata>
class RomaV8AppService {
 public:
  using RomaService =
      google::scp::roma::sandbox::roma_service::RomaService<TMetadata>;
  using Config = google::scp::roma::Config<TMetadata>;

  explicit RomaV8AppService(Config config, std::string_view code_id)
      : roma_service_(std::make_unique<RomaService>(std::move(config))),
        code_id_(code_id) {}

  // RomaV8AppService is movable.
  RomaV8AppService(RomaV8AppService&&) = default;
  RomaV8AppService& operator=(RomaV8AppService&&) = default;

  // RomaV8AppService is not copyable.
  RomaV8AppService(const RomaV8AppService&) = delete;
  RomaV8AppService& operator=(const RomaV8AppService&) = delete;

  virtual ~RomaV8AppService() {
    if (roma_service_) {
      roma_service_->Stop().IgnoreError();
    }
  }

  RomaService* GetRomaService() { return roma_service_.get(); }

  void Cancel(const ExecutionToken& token) { roma_service_->Cancel(token); }

  /*
   * Args:
   *   notification --
   *   notify_status -- status of the registration, check this value after
   *     notification is triggered
   *   jscode --
   *   code_version --
   */
  absl::Status Register(std::string_view jscode, std::string_view code_version,
                        absl::Notification& notification,
                        absl::Status& notify_status) {
    code_version_ = code_version;
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = code_id_,
        .version_string = std::string(code_version),
        .js = std::string(jscode),
    });
    auto cb = [&notification,
               &notify_status](absl::StatusOr<ResponseObject> resp) {
      notify_status = resp.status();
      notification.Notify();
    };
    return roma_service_->LoadCodeObj(std::move(code_obj), std::move(cb));
  }

  template <typename TRequest, typename TResponse>
  absl::StatusOr<ExecutionToken> Execute(
      absl::Notification& notification, std::string_view handler_fn_name,
      const TRequest& request,
      absl::StatusOr<std::unique_ptr<TResponse>>& response,
      TMetadata metadata = TMetadata()) {
    PS_ASSIGN_OR_RETURN(std::string encoded_request,
                        google::scp::roma::romav8::Encode(request));
    InvocationStrRequest<TMetadata> execution_obj = {
        .id = code_id_,
        .version_string = std::string(code_version_),
        .handler_name = std::string(handler_fn_name),
        .input = {encoded_request},
        .treat_input_as_byte_str = true,
        .metadata = std::move(metadata),
    };
    auto execute_cb = [&response,
                       &notification](absl::StatusOr<ResponseObject> resp) {
      if (resp.ok()) {
        auto resp_ptr = std::make_unique<TResponse>();
        if (absl::Status decode =
                google::scp::roma::romav8::Decode(resp->resp, *resp_ptr);
            decode.ok()) {
          response = std::move(resp_ptr);
        } else {
          const std::string error_msg =
              absl::StrCat("Error decoding response. response: ", resp->resp);
          LOG(ERROR) << error_msg;
          response = absl::InternalError(error_msg);
        }
      } else {
        LOG(ERROR) << "Error in Roma Execute()";
        response = resp.status();
      }
      notification.Notify();
    };
    return roma_service_->Execute(
        std::make_unique<InvocationStrRequest<TMetadata>>(
            std::move(execution_obj)),
        std::move(execute_cb));
  }

  template <typename TRequest, typename TResponse>
  absl::StatusOr<ExecutionToken> Execute(
      absl::AnyInvocable<void(absl::StatusOr<TResponse>)> callback,
      std::string_view handler_fn_name, const TRequest& request,
      TMetadata metadata = TMetadata()) {
    LOG(INFO) << "code id: " << code_id_;
    LOG(INFO) << "code version: " << code_version_;
    LOG(INFO) << "handler fn: " << handler_fn_name;

    PS_ASSIGN_OR_RETURN(std::string encoded_request,
                        google::scp::roma::romav8::Encode(request));

    InvocationStrRequest<TMetadata> execution_obj = {
        .id = code_id_,
        .version_string = std::string(code_version_),
        .handler_name = std::string(handler_fn_name),
        .input = {encoded_request},
        .treat_input_as_byte_str = true,
        .metadata = std::move(metadata),
    };

    Callback callback_wrapper =
        [callback =
             std::move(callback)](absl::StatusOr<ResponseObject> resp) mutable {
          absl::StatusOr<TResponse> template_response;
          if (resp.ok()) {
            auto response_obj = TResponse();
            if (absl::Status decode =
                    google::scp::roma::romav8::Decode(resp->resp, response_obj);
                decode.ok()) {
              template_response = std::move(response_obj);
            } else {
              const std::string error_msg = absl::StrCat(
                  "Error decoding response. response: ", resp->resp);
              LOG(ERROR) << error_msg;
              template_response = absl::InternalError(error_msg);
            }
          } else {
            LOG(ERROR) << "Error in Roma Execute()";
            template_response = resp.status();
          }
          callback(std::move(template_response));
        };

    return roma_service_->Execute(
        std::make_unique<InvocationStrRequest<TMetadata>>(
            std::move(execution_obj)),
        std::move(callback_wrapper));
  }

 protected:
  absl::Status Init() { return roma_service_->Init(); }

 private:
  std::unique_ptr<RomaService> roma_service_;

  std::string code_id_;
  std::string code_version_;
};

}  // namespace google::scp::roma::romav8::app_api

#endif  // ROMA_ROMA_SERVICE_ROMAV8_APP_SERVICE_H_
