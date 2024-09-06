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

#include <chrono>
#include <fstream>
#include <string>
#include <string_view>

#include <grpcpp/grpcpp.h>

#include <google/protobuf/util/json_util.h>

#include "absl/base/log_severity.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "src/roma/native_function_grpc_server/interface.h"
#include "src/roma/native_function_grpc_server/proto/test_service.grpc.pb.h"
#include "src/roma/native_function_grpc_server/proto/test_service.pb.h"

ABSL_FLAG(std::string, server_address, "", "Address to connect to GRPC server");
ABSL_FLAG(int32_t, id, -1, "Id of GRPC Client");
ABSL_FLAG(int32_t, num_requests, -1, "Number of GRPC requests to make");
ABSL_FLAG(int32_t, delay_ms, -1,
          "Ms to delay handler from sending back the response");

bool SendRpc(privacy_sandbox::server_common::TestService::Stub* stub,
             privacy_sandbox::server_common::TestMethodRequest& request) {
  privacy_sandbox::server_common::TestMethodResponse response;
  grpc::ClientContext context;
  context.set_wait_for_ready(true);
  context.AddMetadata(std::string(google::scp::roma::grpc_server::kUuidTag),
                      absl::StrCat(absl::GetFlag(FLAGS_id)));

  const grpc::Status status = stub->TestMethod(&context, request, &response);

  if (status.ok()) {
    LOG(INFO) << "Successfully sent request [" << absl::GetFlag(FLAGS_id)
              << "] to gRPC server.";
    LOG(INFO) << "Server Response [" << absl::GetFlag(FLAGS_id)
              << "]: " << response.output();
  } else {
    LOG(ERROR) << "RPC failed [" << absl::GetFlag(FLAGS_id)
               << "]: " << status.error_message();
  }
  return status.ok();
}

bool ValidateFlags() {
  return absl::GetFlag(FLAGS_server_address) != "" &&
         absl::GetFlag(FLAGS_id) != -1 &&
         absl::GetFlag(FLAGS_num_requests) != -1 &&
         absl::GetFlag(FLAGS_delay_ms) != -1;
}

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverity::kInfo);
  absl::ParseCommandLine(argc, argv);

  LOG(INFO) << "RUNNING CLIENT";
  LOG(INFO) << "Validating Flags";
  if (!ValidateFlags()) {
    return -1;
  }

  std::shared_ptr<grpc::Channel> grpc_channel = grpc::CreateChannel(
      absl::GetFlag(FLAGS_server_address), grpc::InsecureChannelCredentials());
  std::unique_ptr<privacy_sandbox::server_common::TestService::Stub> stub(
      privacy_sandbox::server_common::TestService::NewStub(grpc_channel));

  LOG(INFO) << "Building Grpc TestMethodRequest [" << absl::GetFlag(FLAGS_id)
            << "]...";

  privacy_sandbox::server_common::TestMethodRequest request;
  request.set_input("Hello ");
  request.set_processing_delay_ms(absl::GetFlag(FLAGS_delay_ms));

  LOG(INFO) << "Sending " << absl::GetFlag(FLAGS_num_requests)
            << " TestMethodRequest requests [" << absl::GetFlag(FLAGS_id)
            << "] to " << absl::GetFlag(FLAGS_server_address);
  for (int i = 0; i < absl::GetFlag(FLAGS_num_requests); i++) {
    LOG(INFO) << "Sending TestMethodRequest request #" << i;
    if (!SendRpc(stub.get(), request)) {
      return 1;
    }
  }

  return 0;
}
