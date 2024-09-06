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
#include "native_function_grpc_server.h"

#include <gtest/gtest.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/log/log.h"
#include "absl/log/scoped_mock_log.h"
#include "src/roma/config/config.h"
#include "src/roma/metadata_storage/metadata_storage.h"
#include "src/roma/native_function_grpc_server/proto/multi_service.grpc.pb.h"
#include "src/roma/native_function_grpc_server/proto/multi_service.pb.h"
#include "src/roma/native_function_grpc_server/request_handlers.h"
#include "src/roma/native_function_grpc_server/test_request_handlers.h"

using ::testing::_;
constexpr std::string_view kClientPath =
    "src/roma/native_function_grpc_server/grpc_client";
constexpr std::string_view kLoggingClientPath =
    "src/roma/native_function_grpc_server/grpc_logging_client";
constexpr std::string_view kMultiClientPath =
    "src/roma/native_function_grpc_server/grpc_multi_client";

namespace google::scp::roma::grpc_server {
namespace {
using DefaultMetadata = std::string;

std::string CreateSocketAddress() {
  return absl::StrCat(std::tmpnam(nullptr), ".sock");
}

class NativeFunctionGrpcServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    socket_address_ = CreateSocketAddress();
    std::vector<std::string> socket_addresses = {
        absl::StrCat("unix:", socket_address_)};

    metadata_storage_ = std::make_unique<MetadataStorage<DefaultMetadata>>();
    server_ = std::make_unique<NativeFunctionGrpcServer<DefaultMetadata>>(
        metadata_storage_.get(), socket_addresses);
  }

  void PopulateMetadataStorage(int num_processes, int num_iters) {
    for (int iter = 0; iter < num_iters; iter++) {
      for (int i = 0; i < num_processes; i++) {
        std::string uuid = absl::StrCat((iter * num_processes) + i);
        std::string metadata = absl::StrCat("metadata_", uuid);
        ASSERT_TRUE(
            metadata_storage_->Add(std::move(uuid), std::move(metadata)).ok());
      }
    }
  }

  std::unique_ptr<NativeFunctionGrpcServer<DefaultMetadata>> server_;
  std::unique_ptr<MetadataStorage<DefaultMetadata>> metadata_storage_;
  std::string socket_address_;
};

void ExecuteClientBinaries(std::string_view path, std::string_view address,
                           int num_processes, int num_iters) {
  std::vector<pid_t> child_pids;
  const std::string server_address = absl::StrCat("unix:", address);
  for (int iter = 0; iter < num_iters; iter++) {
    for (int i = 0; i < num_processes; i++) {
      const pid_t pid = fork();
      ASSERT_NE(pid, -1) << "Fork failed!";
      if (pid == 0) {
        const int id = (iter * num_processes) + i;
        /** Calculate a delay in milliseconds to ensure nontrivial concurrency
         * among child processes. Mods were chosen so that roughly 80% of all
         *child processes would have no delay or a delay of 40ms, 15% would
         * have a delay of 120ms, and 5% would have a delay of 400ms.
         *
         * For example, for id % 13 == 0, [1 / (13 + 1)] = 7% (approximately 5%
         * to account for multiples of 13 that are also multiples of 3 or 4).
         */
        const std::string delay_ms = (id % 3 == 0)    ? "40"
                                     : (id % 4 == 0)  ? "120"
                                     : (id % 13 == 0) ? "400"
                                                      : "0";

        std::string address_flag =
            absl::StrCat("--server_address=", server_address);
        std::string id_flag = absl::StrCat("--id=", id);
        std::string delay_ms_flag = absl::StrCat("--delay_ms=", delay_ms);
        std::string num_requests_flag = absl::StrCat("--num_requests=", 1);
        const char* flags[] = {
            path.data(),           address_flag.c_str(),
            id_flag.c_str(),       num_requests_flag.c_str(),
            delay_ms_flag.c_str(), nullptr,
        };

        EXPECT_NE(execvp(path.data(), (char* const*)flags), -1)
            << "Failed to execute grpc_client.cc";
        _exit(EXIT_FAILURE);  // Terminate child if exec failed
      } else {
        // Parent process
        child_pids.push_back(pid);
      }
    }
  }

  for (const auto pid : child_pids) {
    int status;
    waitpid(pid, &status, 0);  // Wait for child completion
    EXPECT_EQ(status, 0);
  }
}

void TestBinary(std::string_view path, std::string_view socket_address,
                NativeFunctionGrpcServer<DefaultMetadata>& server,
                int num_processes = 10, int num_iters = 1) {
  ASSERT_EQ(access(path.data(), X_OK), 0);

  LOG(INFO) << "Initializing the server...";
  server.Run();

  ExecuteClientBinaries(path, socket_address, num_processes, num_iters);

  server.Shutdown();
}
}  // namespace

TEST_F(NativeFunctionGrpcServerTest, ServerCanLogByDefault) {
  constexpr int num_processes = 10;
  constexpr int num_iters = 4;
  absl::ScopedMockLog log;
  log.StartCapturingLogs();
  EXPECT_CALL(log,
              Log(absl::LogSeverity::kInfo, testing::_, "Log gRPC called."))
      .Times(num_processes * num_iters);

  PopulateMetadataStorage(num_processes, num_iters);

  // Logging Service and associated factory function are registered by default
  // in RomaService
  Config<DefaultMetadata> config;
  config.RegisterService(std::make_unique<AsyncLoggingService>(),
                         LogHandler<DefaultMetadata>());

  server_->AddServices(config.ReleaseServices());
  server_->AddFactories(config.ReleaseFactories());

  TestBinary(kLoggingClientPath, socket_address_, *server_, num_processes,
             num_iters);

  log.StopCapturingLogs();
}

TEST_F(NativeFunctionGrpcServerTest, ServerCanRegisterRpcHandler) {
  constexpr int num_processes = 2;
  constexpr int num_iters = 2;
  absl::ScopedMockLog log;
  log.StartCapturingLogs();
  EXPECT_CALL(
      log, Log(absl::LogSeverity::kInfo, testing::_, "TestMethod gRPC called."))
      .Times(num_processes * num_iters);

  PopulateMetadataStorage(num_processes, num_iters);

  Config<DefaultMetadata> config;
  config.RegisterService(std::make_unique<AsyncService>(),
                         TestMethodHandler<DefaultMetadata>());

  server_->AddServices(config.ReleaseServices());
  server_->AddFactories(config.ReleaseFactories());

  TestBinary(kClientPath, socket_address_, *server_, num_processes, num_iters);

  log.StopCapturingLogs();
}

TEST_F(NativeFunctionGrpcServerTest, ServerCanRegisterMultipleRpcHandlers) {
  constexpr int num_processes = 2;
  constexpr int num_iters = 2;
  absl::ScopedMockLog log;
  log.StartCapturingLogs();
  EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, testing::_,
                       "TestMethod1 gRPC called."))
      .Times(num_processes * num_iters);
  EXPECT_CALL(log, Log(absl::LogSeverity::kInfo, testing::_,
                       "TestMethod2 gRPC called."))
      .Times(num_processes * num_iters);

  PopulateMetadataStorage(num_processes, num_iters);

  Config<DefaultMetadata> config;
  config.RegisterService(std::make_unique<AsyncMultiService>(),
                         TestMethod1Handler<DefaultMetadata>(),
                         TestMethod2Handler<DefaultMetadata>());

  server_->AddServices(config.ReleaseServices());
  server_->AddFactories(config.ReleaseFactories());

  TestBinary(kMultiClientPath, socket_address_, *server_, num_processes,
             num_iters);
  log.StopCapturingLogs();
}
}  // namespace google::scp::roma::grpc_server
