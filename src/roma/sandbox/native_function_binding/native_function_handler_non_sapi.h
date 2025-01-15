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

#ifndef ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_NATIVE_FUNCTION_HANDLER_NON_SAPI_H_
#define ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_NATIVE_FUNCTION_HANDLER_NON_SAPI_H_

#include <unistd.h>

#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "src/roma/interface/roma.h"
#include "src/roma/logging/logging.h"
#include "src/roma/metadata_storage/metadata_storage.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/native_function_binding/native_function_table.h"
#include "src/roma/sandbox/native_function_binding/rpc_wrapper.pb.h"
#include "src/util/execution_token.h"

using google::scp::roma::sandbox::constants::kRequestId;
using google::scp::roma::sandbox::constants::kRequestUuid;

namespace google::scp::roma::sandbox::native_function_binding {

template <typename TMetadata = google::scp::roma::DefaultMetadata>
class NativeFunctionHandlerNonSapi {
 public:
  /**
   * @brief Construct a new Native Function Handler object
   *
   * @param function_table The function table object to look up function
   * bindings in.
   * @param local_fds The local file descriptors that we will use to listen for
   * function calls.
   * @param remote_fds The remote file descriptors. These are what the remote
   * process uses to send requests to this process.
   */
  NativeFunctionHandlerNonSapi(NativeFunctionTable<TMetadata>* function_table,
                               MetadataStorage<TMetadata>* metadata_storage,
                               const std::vector<int>& local_fds,
                               std::vector<int> remote_fds,
                               bool skip_callback_for_cancelled = true)
      : stop_(false),
        function_table_(function_table),
        metadata_storage_(metadata_storage),
        remote_fds_(std::move(remote_fds)),
        local_fds_(local_fds),
        skip_callback_for_cancelled_(skip_callback_for_cancelled) {}

  void Run() ABSL_LOCKS_EXCLUDED(canceled_requests_mu_) {
    ROMA_VLOG(9) << "Calling native function handler" << std::endl;
    for (int i = 0; i < local_fds_.size(); i++) {
      function_handler_threads_.emplace_back([this, i] {
        while (true) {
          int local_fd = local_fds_.at(i);
          proto::RpcWrapper wrapper_proto;

          // This unblocks once a call is issued from the other side
          char buffer[1024] = {0};
          ssize_t bytesRead = read(local_fd, buffer, sizeof(buffer) - 1);
          if (absl::MutexLock lock(&stop_mutex_); stop_) {
            break;
          }
          if (bytesRead == -1) {
            ROMA_VLOG(9) << "Could Not Receive Message: " << strerror(errno);
            continue;
          }
          std::string serialized_proto = std::string(buffer, bytesRead);
          if (!wrapper_proto.ParseFromString(serialized_proto)) {
            ROMA_VLOG(9) << "Could Not Parse Proto from Serialized String";
            continue;
          }

          auto io_proto = wrapper_proto.mutable_io_proto();
          const auto& invocation_req_uuid = wrapper_proto.request_uuid();
          {
            absl::MutexLock lock(&canceled_requests_mu_);
            if (const auto it = canceled_requests_.find(invocation_req_uuid);
                it != canceled_requests_.end()) {
              // TODO(b/353555061): Avoid execution errors that relate to
              // cancellation.
              io_proto->mutable_errors()->Add(std::string(kRequestCanceled));
              ROMA_VLOG(1) << kRequestCanceled;

              std::string serialized_wrapper =
                  wrapper_proto.SerializeAsString();
              ssize_t sentBytes = write(local_fd, serialized_wrapper.c_str(),
                                        serialized_wrapper.size());
              if (sentBytes == -1) {
                ROMA_VLOG(1)
                    << "Could not send the call to the NativeFunctionInvoker.";
              }

              // Remove the canceled request's execution token from
              // canceled_requests_ to prevent bloat.
              canceled_requests_.erase(it);
              continue;
            }
          }

          // Get function name
          if (const auto function_name = wrapper_proto.function_name();
              !function_name.empty()) {
            if (metadata_storage_ == nullptr) {
              TMetadata dummy_metadata;
              if (FunctionBindingPayload<TMetadata> wrapper{
                      *io_proto,
                      dummy_metadata,
                  };
                  !function_table_->Call(function_name, wrapper).ok()) {
                // If execution failed, add errors to the proto to return
                io_proto->mutable_errors()->Add(
                    std::string(kFailedNativeHandlerExecution));
                ROMA_VLOG(1) << kFailedNativeHandlerExecution;
              }
            } else if (auto reader = ScopedValueReader<TMetadata>::Create(
                           metadata_storage_->GetMetadataMap(),
                           invocation_req_uuid);
                       !reader.ok()) {
              // If mutex can't be found, add errors to the proto to return
              io_proto->mutable_errors()->Add(std::string(kCouldNotFindMutex));
              ROMA_VLOG(1) << kCouldNotFindMutex;
            } else if (auto value = reader->Get(); !value.ok()) {
              // If metadata can't be found, add errors to the proto to return
              io_proto->mutable_errors()->Add(
                  std::string(kCouldNotFindMetadata));
              ROMA_VLOG(1) << kCouldNotFindMetadata;
            } else if (FunctionBindingPayload<TMetadata> wrapper{
                           *io_proto,
                           **value,
                       };
                       !function_table_->Call(function_name, wrapper).ok()) {
              // If execution failed, add errors to the proto to return
              io_proto->mutable_errors()->Add(
                  std::string(kFailedNativeHandlerExecution));
              ROMA_VLOG(1) << kFailedNativeHandlerExecution;
            }
          } else {
            // If we can't find the function, add errors to the proto to return
            io_proto->mutable_errors()->Add(
                std::string(kCouldNotFindFunctionName));
            ROMA_VLOG(1) << kCouldNotFindFunctionName;
          }

          std::string serialized_wrapper = wrapper_proto.SerializeAsString();
          ssize_t sentBytes = write(local_fd, serialized_wrapper.c_str(),
                                    serialized_wrapper.size());
          if (sentBytes == -1) {
            ROMA_VLOG(1)
                << "Could not send the call to the NativeFunctionInvoker.";
            continue;
          }
        }
      });
    }
  }

  void Stop() {
    {
      absl::MutexLock lock(&stop_mutex_);
      stop_ = true;
    }

    // We write to the comms object so that we can unblock the function binding
    // threads waiting on it.
    for (const int fd : remote_fds_) {
      char byte = 'S';  // 'S' for Stop
      ssize_t sentBytes = write(fd, &byte, 1);
      if (sentBytes == -1) {
        ROMA_VLOG(9) << "Could not send the call to the NativeFunctionHandler."
                     << std::endl;
      }
      close(fd);
    }
    // Wait for the function binding threads to stop before terminating the
    // comms object below.
    for (auto& t : function_handler_threads_) {
      if (t.joinable()) {
        t.join();
      }
    }

    for (int fd : local_fds_) {
      close(fd);
    }
  }

  void PreventCallbacks(ExecutionToken token)
      ABSL_LOCKS_EXCLUDED(canceled_requests_mu_) {
    absl::MutexLock lock(&canceled_requests_mu_);
    canceled_requests_.insert(std::move(token).value);
  }

 private:
  static constexpr std::string_view kFailedNativeHandlerExecution =
      "ROMA: Failed to execute the C++ function.";
  static constexpr std::string_view kRequestCanceled =
      "ROMA: Execution for request has been canceled.";
  static constexpr std::string_view kCouldNotFindFunctionName =
      "ROMA: Could not find C++ function by name.";
  static constexpr std::string_view kCouldNotFindMetadata =
      "ROMA: Could not find metadata associated with C++ function.";
  static constexpr std::string_view kCouldNotFindMutex =
      "ROMA: Could not find mutex for metadata associated with C++ function.";

  bool stop_ ABSL_GUARDED_BY(stop_mutex_);
  absl::Mutex stop_mutex_;

  NativeFunctionTable<TMetadata>* function_table_;
  // Map of invocation request uuid to associated metadata.
  MetadataStorage<TMetadata>* metadata_storage_;
  std::vector<std::thread> function_handler_threads_;
  absl::flat_hash_set<std::string> canceled_requests_
      ABSL_GUARDED_BY(canceled_requests_mu_);
  absl::Mutex canceled_requests_mu_;
  // We need the remote file descriptors to unblock the local ones when stopping
  std::vector<int> remote_fds_;
  std::vector<int> local_fds_;
  bool skip_callback_for_cancelled_;
};

template <typename T>
using NativeFunctionHandler = NativeFunctionHandlerNonSapi<T>;
}  // namespace google::scp::roma::sandbox::native_function_binding

#endif  // ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_NATIVE_FUNCTION_HANDLER_NON_SAPI_H_
