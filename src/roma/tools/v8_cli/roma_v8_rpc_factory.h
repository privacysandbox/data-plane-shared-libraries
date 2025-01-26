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

#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/byob/benchmark/latency_formatter.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"
#include "src/util/duration.h"
#include "src/util/execution_token.h"
#include "src/util/status_macro/status_macros.h"

using ExecutionFunc = absl::AnyInvocable<void(
    privacy_sandbox::server_common::Stopwatch, absl::StatusOr<absl::Duration>*,
    absl::StatusOr<std::string>*, absl::Notification*)>;
using CleanupFunc = absl::AnyInvocable<void()>;

namespace google::scp::roma::tools::v8_cli {

using RomaV8Service = google::scp::roma::sandbox::roma_service::RomaService<>;

std::unique_ptr<FunctionBindingObjectV2<>> CreateFunctionBindingObjectV2(
    std::string_view function_name,
    std::function<void(FunctionBindingPayload<>&)> function) {
  return std::make_unique<FunctionBindingObjectV2<>>(FunctionBindingObjectV2<>{
      .function_name = std::string(function_name),
      .function = function,
  });
}

void StringOutFunction(FunctionBindingPayload<>& wrapper) {
  wrapper.io_proto.set_output_string("Function from C++");
}

void SleepFunction(FunctionBindingPayload<>& wrapper) {
  absl::Duration duration;
  CHECK(absl::ParseDuration(wrapper.io_proto.input_string(), &duration));
  privacy_sandbox::server_common::Stopwatch stopwatch;
  absl::SleepFor(duration);
  const auto elapsed = stopwatch.GetElapsedTime();
  const auto start = stopwatch.GetStartTime();
  const auto end = start + elapsed;
  LOG(INFO) << "Start Time: " << start;
  LOG(INFO) << "End Time: " << end;
  wrapper.io_proto.set_output_string(absl::StrCat(elapsed));
}

std::string GetUDF(std::string_view udf_file_path) {
  LOG(INFO) << "Loading UDF from file \"" << udf_file_path << "\"...";
  std::ifstream input_str(udf_file_path.data());
  if (!input_str.is_open()) {
    LOG(FATAL) << "Failed to open UDF file: " << udf_file_path;
  }
  return std::string((std::istreambuf_iterator<char>(input_str)),
                     (std::istreambuf_iterator<char>()));
}

// If batch_execute is true, burst_size requests will be bundled and executed
// using RomaService::BatchExecute Otherwise, burst_size requests will be
// executed individually using RomaService::Execute
std::pair<ExecutionFunc, CleanupFunc> CreateV8RpcFunc(
    int num_workers, std::string_view udf_path, std::string_view handler_name,
    const std::vector<std::string>& input_args,
    std::atomic<std::int64_t>& completions, int burst_size = 1,
    bool batch_execute = false) {
  CHECK(!udf_path.empty()) << "UDF path must be specified in V8 mode";
  CHECK(!handler_name.empty()) << "Handler name must be specified in V8 mode";

  RomaV8Service::Config config;
  config.number_of_workers = num_workers;
  auto logging_fn = [](absl::LogSeverity severity,
                       const RomaV8Service::TMetadata& metadata,
                       std::string_view msg) { LOG(LEVEL(severity)) << msg; };
  config.SetLoggingFunction(std::move(logging_fn));
  config.RegisterFunctionBinding(
      CreateFunctionBindingObjectV2("callback", StringOutFunction));
  config.RegisterFunctionBinding(
      CreateFunctionBindingObjectV2("sleep", SleepFunction));

  LOG(INFO) << "Initializing RomaService with " << num_workers << " workers...";
  auto roma_service = std::make_unique<RomaV8Service>(std::move(config));
  CHECK_OK(roma_service->Init());

  const std::string js_code = GetUDF(udf_path);
  const std::string version = "v1";
  const google::scp::roma::CodeObject code_object = {
      .id = google::scp::core::common::ToString(
          google::scp::core::common::Uuid::GenerateUuid()),
      .version_string = version,
      .js = js_code,
  };

  absl::Notification load_finished;
  CHECK_OK(roma_service->LoadCodeObj(
      std::make_unique<google::scp::roma::CodeObject>(code_object),
      [&load_finished](absl::StatusOr<google::scp::roma::ResponseObject> resp) {
        CHECK_OK(resp);
        load_finished.Notify();
      }));
  load_finished.WaitForNotification();
  LOG(INFO) << "UDF loaded successfully";

  std::vector<google::scp::roma::InvocationStrRequest<>> execution_objects;
  std::vector<std::string> escaped_input_args;
  for (const auto& arg : input_args) {
    escaped_input_args.push_back(absl::StrCat("\"", arg, "\""));
  }
  for (int i = 0; i < burst_size; i++) {
    execution_objects.push_back({
        .id = google::scp::core::common::ToString(
            google::scp::core::common::Uuid::GenerateUuid()),
        .version_string = "v1",
        .handler_name = std::string(handler_name),
        .input = escaped_input_args,
    });
  }

  const auto execute_rpc_func =
      [roma_service = roma_service.get(), execution_objects, &completions](
          privacy_sandbox::server_common::Stopwatch stopwatch,
          absl::StatusOr<absl::Duration>* duration,
          absl::StatusOr<std::string>* output,
          absl::Notification* done) mutable {
        absl::StatusOr<google::scp::roma::ExecutionToken> exec_token =
            roma_service->Execute(
                std::make_unique<google::scp::roma::InvocationStrRequest<>>(
                    execution_objects[0]),
                [duration, output, stopwatch = std::move(stopwatch), done,
                 &completions](
                    absl::StatusOr<google::scp::roma::ResponseObject> resp) {
                  if (resp.ok()) {
                    *duration = stopwatch.GetElapsedTime();
                    *output = resp->resp.substr(1, resp->resp.size() - 2);
                  } else {
                    *duration = std::move(resp.status());
                    *output = duration->status();
                  }
                  completions++;
                  done->Notify();
                });

        if (!exec_token.ok()) {
          *duration = exec_token.status();
          *output = duration->status();
          completions++;
          done->Notify();
          return;
        }
      };

  const auto batch_execute_rpc_func =
      [roma_service = roma_service.get(),
       execution_objects = std::move(execution_objects),
       &completions](privacy_sandbox::server_common::Stopwatch stopwatch,
                     absl::StatusOr<absl::Duration>* duration,
                     absl::StatusOr<std::string>* output,
                     absl::Notification* done) mutable {
        absl::Status status = roma_service->BatchExecute(
            execution_objects,
            [duration, output, stopwatch = std::move(stopwatch), done,
             &completions](
                const std::vector<absl::StatusOr<ResponseObject>>& batch_resp) {
              if (batch_resp[0].ok()) {
                *duration = stopwatch.GetElapsedTime();
                *output = privacy_sandbox::server_common::byob::
                    LatencyFormatter::Stringify(batch_resp);
              } else {
                *duration = std::move(batch_resp[0].status());
                *output = duration->status();
              }
              completions += batch_resp.size();
              done->Notify();
            });

        if (!status.ok()) {
          std::cout << status.message() << std::endl;
          *duration = status;
          *output = status;
          completions++;
          done->Notify();
          return;
        }
      };

  ExecutionFunc rpc_func = std::move(execute_rpc_func);
  if (batch_execute) {
    LOG(INFO) << "Batching enabled";
    rpc_func = std::move(batch_execute_rpc_func);
  }

  auto callback = [roma_service = std::move(roma_service)]() {
    privacy_sandbox::server_common::Stopwatch stopwatch;
    CHECK_OK(roma_service->Stop());
    LOG(INFO) << "Roma shutdown duration: " << stopwatch.GetElapsedTime();
  };

  return std::make_pair(std::move(rpc_func), std::move(callback));
}

}  // namespace google::scp::roma::tools::v8_cli
