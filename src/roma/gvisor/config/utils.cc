// Copyright 2024 Google LLC
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

#include "src/roma/gvisor/config/utils.h"

#include <math.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "src/proto/grpc/health/v1/health.grpc.pb.h"

#define __DEFINE_AS_STRING(var) #var
#define DEFINE_AS_STRING(var) __DEFINE_AS_STRING(var)

namespace privacy_sandbox::server_common::gvisor {

namespace {

constexpr uint32_t kRetryBackoffBase = 2;
constexpr uint32_t kMaxRetries = 10;

}  // namespace

std::filesystem::path GetRomaContainerDir() {
  return DEFINE_AS_STRING(ROMA_CONTAINER_DIR);
}

std::filesystem::path GetRomaContainerRootDir() {
  return DEFINE_AS_STRING(ROMA_CONTAINER_ROOT_DIR);
}

std::filesystem::path GetRomaServerPwd() {
  return DEFINE_AS_STRING(ROMA_SERVER_PWD);
}

std::string GetLibMounts() { return ROMA_LIB_MOUNTS; }

std::filesystem::path GetRunscPath() { return DEFINE_AS_STRING(RUNSC_PATH); }

absl::StatusOr<std::string> CreateUniqueDirectory() {
  char tmp_dir[] = "/tmp/roma_app_XXXXXX";
  char* dir_name = ::mkdtemp(tmp_dir);
  if (dir_name == nullptr) {
    return absl::ErrnoToStatus(
        errno, absl::StrCat("Could not create directory ", dir_name));
  }
  return std::string(dir_name);
}

absl::StatusOr<std::filesystem::path> CreateUniqueSocketName() {
  char tmp_file[] = "/tmp/socket_dir_XXXXXX";
  char* socket_dir = ::mkdtemp(tmp_file);
  if (socket_dir == nullptr) {
    return absl::ErrnoToStatus(
        errno, absl::StrCat("Could not create socket directory ", tmp_file));
  }
  if (char* socket_pwd = ::tempnam(socket_dir, nullptr);
      socket_pwd != nullptr) {
    return std::string(socket_pwd);
  }
  return absl::ErrnoToStatus(
      errno,
      absl::StrCat("Could not create socket file in directory ", socket_dir));
}

absl::Status HealthCheckWithExponentialBackoff(
    const std::shared_ptr<::grpc::Channel>& channel) {
  std::unique_ptr<::grpc::health::v1::Health::Stub> health_stub =
      ::grpc::health::v1::Health::NewStub(channel);
  ::grpc::health::v1::HealthCheckRequest request;
  request.set_service("");

  int retries = 0;
  while (retries < kMaxRetries) {
    int64_t backoff = ::pow(double(kRetryBackoffBase), double(retries));
    retries++;
    ::sleep(backoff);

    ::grpc::ClientContext context;
    ::grpc::health::v1::HealthCheckResponse response;
    if (health_stub->Check(&context, request, &response).error_code() ==
        ::grpc::StatusCode::OK) {
      return absl::OkStatus();
    }
  }
  return absl::DeadlineExceededError("Server did not startup in time");
}

}  // namespace privacy_sandbox::server_common::gvisor
