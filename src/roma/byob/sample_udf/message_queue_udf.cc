// Copyright 2025 Google LLC
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

#include <errno.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"

using ::google::protobuf::io::FileInputStream;
using ::privacy_sandbox::roma_byob::example::FUNCTION_READ_SYS_V_MESSAGE_QUEUE;
using ::privacy_sandbox::roma_byob::example::FUNCTION_WRITE_SYS_V_MESSAGE_QUEUE;
using ::privacy_sandbox::roma_byob::example::SampleRequest;
using ::privacy_sandbox::roma_byob::example::SampleResponse;

constexpr char kMessage[] = "Hello from System V message queue IPC.";
constexpr int kMessageKey = 1234;

SampleRequest ReadRequestFromFd(int fd) {
  SampleRequest req;
  FileInputStream stream(fd);
  google::protobuf::util::ParseDelimitedFromZeroCopyStream(&req, &stream,
                                                           nullptr);
  return req;
}

void WriteResponseToFd(int fd, SampleResponse resp) {
  google::protobuf::util::SerializeDelimitedToFileDescriptor(resp, fd);
}

absl::Status SendMessage(int message_queue_id) {
  if (::msgsnd(message_queue_id, kMessage, sizeof(kMessage), IPC_NOWAIT) ==
      -1) {
    return absl::ErrnoToStatus(errno, "Failed to send message");
  }
  return absl::OkStatus();
}

absl::StatusOr<std::string> ReceiveMessage(int message_queue_id) {
  char received_message[1024];
  if (::msgrcv(message_queue_id, &received_message, 1024, 0,
               MSG_NOERROR | IPC_NOWAIT) == -1) {
    if (errno != ENOMSG) {
      return absl::ErrnoToStatus(errno, "Failed to receive message");
    }
    return absl::UnavailableError("Could not find a message in the queue");
  }
  return received_message;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Not enough arguments!" << std::endl;
    return -1;
  }
  int fd = std::stoi(argv[1]);
  if (::write(fd, "a", /*count=*/1) != 1) {
    std::cerr << "Failed to write" << std::endl;
    return -1;
  }
  SampleRequest bin_request = ReadRequestFromFd(fd);

  SampleResponse bin_response;
  int qid = ::msgget(kMessageKey, IPC_CREAT | 0666);
  std::cout << "Queue ID: " << qid << std::endl;

  switch (bin_request.function()) {
    case FUNCTION_WRITE_SYS_V_MESSAGE_QUEUE: {
      if (auto status = SendMessage(qid); !status.ok()) {
        bin_response.set_greeting(status.message());
      } else {
        bin_response.set_greeting(kMessage);
      }
    } break;
    case FUNCTION_READ_SYS_V_MESSAGE_QUEUE: {
      auto resp = ReceiveMessage(qid);
      if (!resp.ok()) {
        bin_response.set_greeting(resp.status().message());
      } else {
        bin_response.set_greeting(*std::move(resp));
      }
    } break;
    default:
      return -1;
  }
  WriteResponseToFd(fd, bin_response);
  exit(EXIT_SUCCESS);
}
