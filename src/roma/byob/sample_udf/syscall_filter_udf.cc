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

#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>

#include <iostream>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"

using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using ::google::protobuf::util::SerializeDelimitedToFileDescriptor;
using ::privacy_sandbox::roma_byob::example::
    FUNCTION_RELOADER_LEVEL_SYSCALL_FILTERING;
using ::privacy_sandbox::roma_byob::example::
    FUNCTION_WORKER_AND_RELOADER_LEVEL_SYSCALL_FILTERING;
using ::privacy_sandbox::roma_byob::example::SampleRequest;
using ::privacy_sandbox::roma_byob::example::SampleResponse;

constexpr char kMessage[] = "Hello from System V message queue IPC.";
constexpr int kMessageKey = 1234;

SampleRequest ReadRequestFromFd(int fd) {
  SampleRequest bin_request;
  FileInputStream input(fd);
  ParseDelimitedFromZeroCopyStream(&bin_request, &input, nullptr);
  return bin_request;
}

void WriteResponseToFd(int fd, SampleResponse resp) {
  google::protobuf::util::SerializeDelimitedToFileDescriptor(resp, fd);
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
  auto bin_request = ReadRequestFromFd(fd);
  SampleResponse bin_response;
  char received_message[1024];
  // Validates that all System V IPC Syscalls are blocked with a permission
  // error by syscall_filtering.
  if (::msgget(/*key=*/kMessageKey, /*msgflg=*/IPC_CREAT | 0666) != -1 ||
      errno != EPERM) {
    bin_response.set_greeting("Failed to block msgget with correct error.");
  } else if (::msgsnd(/*msqid=*/kMessageKey, /*msgp=*/kMessage,
                      /*msgsz=*/sizeof(kMessage), /*msgflg=*/IPC_NOWAIT) >= 0 ||
             errno != EPERM) {
    bin_response.set_greeting("Failed to block msgsnd with correct error.");
  } else if (::msgrcv(/*msqid=*/kMessageKey, /*msgp=*/&received_message,
                      /*msgsz=*/1024, /*msgtyp=*/0,
                      /*msgflg=*/MSG_NOERROR | IPC_NOWAIT) != -1 ||
             errno != EPERM) {
    bin_response.set_greeting("Failed to block msgrcv with correct error.");
  } else if (::msgctl(/*msqid=*/kMessageKey, /*cmd=*/IPC_RMID,
                      /*buf=*/nullptr) != -1 ||
             errno != EPERM) {
    bin_response.set_greeting("Failed to block msgctl with correct error.");
  } else if (::semget(/*key=*/kMessageKey, /*nsems=*/0, /*semflg=*/0600) !=
                 -1 ||
             errno != EPERM) {
    bin_response.set_greeting("Failed to block semget with correct error.");
  } else if (::semop(/*semid=*/kMessageKey, /*sops=*/nullptr, /*nsops=*/2) !=
                 -1 ||
             errno != EPERM) {
    bin_response.set_greeting("Failed to block semop with correct error.");
  } else if (::semctl(/*semid=*/kMessageKey, /*semnum=*/0, /*cmd=*/IPC_STAT) !=
                 -1 ||
             errno != EPERM) {
    bin_response.set_greeting("Failed to block semctl with correct error.");
  } else if (::shmget(/*key=*/kMessageKey, /*size=*/4096,
                      /*shmflg=*/0644 | IPC_CREAT) != -1 ||
             errno != EPERM) {
    bin_response.set_greeting("Failed to block shmget with correct error.");
  } else if (::shmat(kMessageKey, received_message, 0) != (void*)-1 ||
             errno != EPERM) {
    bin_response.set_greeting("Failed to block shmat with correct error.");
  } else if (::shmdt(/*shmaddr=*/nullptr) != -1 || errno != EPERM) {
    bin_response.set_greeting("Failed to block shmdt with correct error.");
  } else if (::shmctl(/*shmid=*/kMessageKey, /*cmd=*/IPC_RMID,
                      /*buf=*/nullptr) != -1 ||
             errno != EPERM) {
    bin_response.set_greeting("Failed to block shmctl with correct error.");
  } else if (bin_request.function() ==
                 FUNCTION_RELOADER_LEVEL_SYSCALL_FILTERING &&
             dup2(/*oldfd=*/1, /*newfd=*/2) < 0) {
    // In case of a reloader-level filter, dup2 is enabled and hence the call
    // should have been successful.
    bin_response.set_greeting(
        "Failed to call dup2 for reloader-level syscall filter.");
  } else if (bin_request.function() ==
                 FUNCTION_WORKER_AND_RELOADER_LEVEL_SYSCALL_FILTERING &&
             (dup2(/*oldfd=*/1, /*newfd=*/2) != -1 || errno != EPERM)) {
    // In case of a worker-and-reloader-level filter, dup2 is disabled and hence
    // the call should have failed with a permission error.
    bin_response.set_greeting(
        "Failed to block dup2 with correct error for worker and reloader-level "
        "syscall filter.");
  } else {
    bin_response.set_greeting(
        "Blocked all System V IPC syscalls and handled syscall filters "
        "correctly.");
  }
  WriteResponseToFd(fd, std::move(bin_response));
  return 0;
}
