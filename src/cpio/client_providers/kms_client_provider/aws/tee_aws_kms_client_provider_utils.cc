/*
 * Copyright 2022 Google LLC
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

#include "tee_aws_kms_client_provider_utils.h"

#include <sys/wait.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"

namespace google::scp::cpio::client_providers::utils {

absl::StatusOr<std::string> Exec(absl::Span<const char* const> args) noexcept {
  int fd[2];
  if (::pipe(fd) == -1) {
    return absl::ErrnoToStatus(errno, "Exec failed to create a pipe.");
  }
  const ::pid_t pid = fork();
  if (pid == 0) {
    ::close(fd[0]);

    // Redirect child standard output to pipe and execute.
    if (::dup2(fd[1], STDOUT_FILENO) == -1 ||
        ::execv(args[0], const_cast<char* const*>(&args[0])) == -1) {
      std::exit(errno);
    }
  } else if (pid == -1) {
    return absl::ErrnoToStatus(errno, "Exec failed to fork a child.");
  }

  // Only parent gets here (pid > 0).
  ::close(fd[1]);
  if (int status; ::waitpid(pid, &status, /*options=*/0) == -1) {
    return absl::ErrnoToStatus(errno, "Exec failed to wait for child.");
  } else if (!WIFEXITED(status)) {
    return absl::InternalError(absl::StrCat(
        "Exec child process exited abnormally while executing command '",
        absl::StrJoin(args, " "), "'"));
  } else if (const int child_errno = WEXITSTATUS(status);
             child_errno != EXIT_SUCCESS) {
    return absl::ErrnoToStatus(
        child_errno,
        absl::StrCat("Exec child process exited with non-zero error number ",
                     child_errno, " while executing command '",
                     absl::StrJoin(args, " "), "'"));
  }
  std::string result;
  if (FILE* const stream = ::fdopen(fd[0], "r"); stream != nullptr) {
    std::array<char, 1024> buffer;
    while (std::fgets(buffer.data(), buffer.size(), stream) != nullptr) {
      result += buffer.data();
    }
  } else {
    return absl::ErrnoToStatus(errno, "Exec failed to read from pipe.");
  }
  return result;
}

}  // namespace google::scp::cpio::client_providers::utils
