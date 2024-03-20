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

#include <google/protobuf/message_lite.h>
#include <google/protobuf/util/json_util.h>

#include "absl/synchronization/notification.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

using google::scp::roma::sandbox::roma_service::RomaService;

namespace google::scp::roma::romav8::app_api {

using TEncoded = std::string;

template <typename T>
absl::StatusOr<TEncoded> Encode(const T& obj) {
  static_assert(std::is_base_of<google::protobuf::MessageLite, T>::value,
                "T must be derived from google::protobuf::MessageLite");
  if (std::string s; obj.SerializeToString(&s)) {
    return s;
  }
  return absl::UnknownError("unable to serialize protobuf object");
}

template <typename T>
absl::Status Decode(const TEncoded& encoded, T& decoded) {
  static_assert(std::is_base_of<google::protobuf::MessageLite, T>::value,
                "T must be derived from google::protobuf::MessageLite");
  if (T obj; obj.ParseFromString(encoded)) {
    decoded.CheckTypeAndMergeFrom(obj);
    return absl::OkStatus();
  }
  return absl::UnknownError("unable to parse protobuf object");
}

template <typename TMetadata = google::scp::roma::DefaultMetadata>
class RomaV8AppService {
 public:
  using RomaService =
      google::scp::roma::sandbox::roma_service::RomaService<TMetadata>;
  using Config = google::scp::roma::Config<TMetadata>;

  RomaV8AppService(Config config, std::string_view code_id)
      : code_id_(code_id) {
    roma_service_ = std::make_unique<RomaService>(std::move(config));
  }

  RomaV8AppService(RomaV8AppService&& other) : code_id_(other.code_id_) {
    roma_service_ = std::move(other.roma_service_);
  }

  RomaV8AppService& operator=(RomaV8AppService&& other) = delete;
  RomaV8AppService(const RomaV8AppService& other) = delete;

  virtual ~RomaV8AppService() {
    if (roma_service_) {
      (void)roma_service_->Stop();
    }
  }

  /*
   * Args:
   *   notification --
   *   notify_status -- status of the registration, check this value after
   *     notification is triggered
   *   jscode --
   *   code_version --
   */
  absl::Status Register(absl::Notification& notification,
                        absl::Status& notify_status, std::string_view jscode,
                        std::string_view code_version = "1") {
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
  absl::Status Execute(absl::Notification& notification,
                       std::string_view handler_fn_name,
                       const TRequest& request, TResponse& response) {
    LOG(INFO) << "code id: " << code_id_;
    LOG(INFO) << "code version: " << code_version_;
    LOG(INFO) << "handler fn: " << handler_fn_name;
    InvocationStrRequest<TMetadata> execution_obj = {
        .id = code_id_,
        .version_string = std::string(code_version_),
        .handler_name = std::string(handler_fn_name),
        .input = {*Encode(request)},
        .treat_input_as_byte_str = true,
    };
    auto execute_cb = [&response,
                       &notification](absl::StatusOr<ResponseObject> resp) {
      if (resp.ok()) {
        if (auto decode = Decode(resp->resp, response); !decode.ok()) {
          LOG(ERROR) << "error decoding response. response: " << resp->resp;
        }
      } else {
        LOG(ERROR) << "Error in Roma Execute()";
      }
      notification.Notify();
    };
    return roma_service_->Execute(
        std::make_unique<InvocationStrRequest<TMetadata>>(
            std::move(execution_obj)),
        std::move(execute_cb));
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
