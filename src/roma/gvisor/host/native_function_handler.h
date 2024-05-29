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

#ifndef SRC_EXPERIMENTAL_ASHRUTI_ROMA_GVISOR_NATIVE_FUNCTION_HANDLER_H_
#define SRC_EXPERIMENTAL_ASHRUTI_ROMA_GVISOR_NATIVE_FUNCTION_HANDLER_H_

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/gvisor/host/callback.pb.h"
#include "src/roma/gvisor/host/uuid.pb.h"
#include "src/roma/metadata_storage/metadata_storage.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/native_function_binding/native_function_table.h"

namespace privacy_sandbox::server_common::gvisor {
class NativeFunctionHandler {
 private:
  using TMetadata = google::scp::roma::DefaultMetadata;
  static constexpr std::string_view kFailedNativeHandlerExecution =
      "ROMA: Failed to execute the C++ function.";
  static constexpr std::string_view kCouldNotFindFunctionName =
      "ROMA: Could not find C++ function by name.";
  static constexpr std::string_view kCouldNotFindMetadata =
      "ROMA: Could not find metadata associated with C++ function.";
  static constexpr std::string_view kCouldNotFindMutex =
      "ROMA: Could not find mutex for metadata associated with C++ function.";

 public:
  /**
   * @brief Construct a new Native Function Handler Sapi Ipc object
   * @param socket_name socket file to listen on. Deletes fail and parent in
   * destructor.
   */
  NativeFunctionHandler(
      std::vector<google::scp::roma::FunctionBindingObjectV2<TMetadata>>
          function_bindings,
      std::string socket_name)
      : stop_(false),
        server_fd_(::socket(AF_UNIX, SOCK_STREAM, 0)),
        socket_name_(std::move(socket_name)) {
    LOG(INFO) << "Starting native function handler";
    PCHECK(server_fd_ != -1);
    {
      sockaddr_un sa;
      ::memset(&sa, 0, sizeof(sa));
      sa.sun_family = AF_UNIX;
      ::strncpy(sa.sun_path, socket_name_.c_str(), sizeof(sa.sun_path));
      PCHECK(::bind(server_fd_, reinterpret_cast<sockaddr*>(&sa),
                    SUN_LEN(&sa)) == 0);
      PCHECK(::listen(server_fd_, /*backlog=*/0) == 0);
    }
    for (auto& function_binding : function_bindings) {
      CHECK_OK(
          function_table_.Register(std::move(function_binding.function_name),
                                   std::move(function_binding.function)));
    }
    thread_.emplace([this] {
      while (true) {
        const int fd = ::accept(server_fd_, nullptr, nullptr);
        if (absl::MutexLock lock(&stop_mutex_); stop_) {
          break;
        }
        PCHECK(fd != -1);
        std::thread(&NativeFunctionHandler::HandlerImpl, this, fd).detach();
      }
    });
  }

  ~NativeFunctionHandler() {
    {
      absl::MutexLock lock(&stop_mutex_);
      stop_ = true;
    }
    PCHECK(::shutdown(server_fd_, SHUT_RDWR) == 0);
    PCHECK(::close(server_fd_) == 0);
    if (thread_->joinable()) {
      thread_->join();
    }
    function_table_.Clear();
    const std::filesystem::path dir =
        std::filesystem::path(socket_name_).parent_path();
    if (std::error_code ec; std::filesystem::remove_all(dir, ec) <= 0) {
      LOG(ERROR) << "Failed to delete " << dir << ": " << ec.message();
    }
  }

  absl::Status StoreMetadata(std::string uuid, TMetadata metadata) {
    return metadata_storage_.Add(std::move(uuid), std::move(metadata));
  }

  absl::Status DeleteMetadata(std::string_view uuid) {
    return metadata_storage_.Delete(uuid);
  }

 private:
  void HandlerImpl(int fd) {
    absl::Cleanup cleanup = [fd] { ::close(fd); };
    Uuid uuid;
    if (google::protobuf::io::FileInputStream input(fd);
        !google::protobuf::util::ParseDelimitedFromZeroCopyStream(&uuid, &input,
                                                                  nullptr)) {
      return;
    }
    CHECK(
        google::protobuf::util::SerializeDelimitedToFileDescriptor(Ack{}, fd));
    while (true) {
      Callback callback;

      // This unblocks once a call is issued from the other side
      if (google::protobuf::io::FileInputStream input(fd);
          !google::protobuf::util::ParseDelimitedFromZeroCopyStream(
              &callback, &input, nullptr)) {
        return;
      }
      auto& io_proto = *callback.mutable_io_proto();

      // Get function name
      if (const std::string& function_name = callback.function_name();
          !function_name.empty()) {
        if (auto reader = ScopedValueReader<TMetadata>::Create(
                metadata_storage_.GetMetadataMap(), uuid.uuid());
            !reader.ok()) {
          // If mutex can't be found, add errors to the proto to return
          io_proto.mutable_errors()->Add(std::string(kCouldNotFindMutex));
          LOG(ERROR) << kCouldNotFindMutex;
        } else if (auto value = reader->Get(); !value.ok()) {
          // If metadata can't be found, add errors to the proto to return
          io_proto.mutable_errors()->Add(std::string(kCouldNotFindMetadata));
          LOG(ERROR) << kCouldNotFindMetadata;
        } else if (google::scp::roma::FunctionBindingPayload<TMetadata> wrapper{
                       io_proto,
                       **value,
                   };
                   !function_table_.Call(function_name, wrapper).ok()) {
          // If execution failed, add errors to the proto to return
          io_proto.mutable_errors()->Add(
              std::string(kFailedNativeHandlerExecution));
          LOG(ERROR) << kFailedNativeHandlerExecution;
        }
      } else {
        // If we can't find the function, add errors to the proto to return
        io_proto.mutable_errors()->Add(std::string(kCouldNotFindFunctionName));
        LOG(ERROR) << kCouldNotFindFunctionName;
      }
      google::protobuf::util::SerializeDelimitedToFileDescriptor(callback, fd);
    }
  }

  absl::Mutex stop_mutex_;
  bool stop_ ABSL_GUARDED_BY(stop_mutex_);
  int server_fd_;
  std::string socket_name_;

  google::scp::roma::sandbox::native_function_binding::NativeFunctionTable<
      TMetadata>
      function_table_;
  // Map of invocation request uuid to associated metadata.
  MetadataStorage<TMetadata> metadata_storage_;
  std::optional<std::thread> thread_;
};
}  // namespace privacy_sandbox::server_common::gvisor

#endif  // SRC_EXPERIMENTAL_ASHRUTI_ROMA_GVISOR_NATIVE_FUNCTION_HANDLER_H_
