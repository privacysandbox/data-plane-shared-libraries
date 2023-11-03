// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <fcntl.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/limits.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#include "absl/strings/str_cat.h"

constexpr std::string_view kLib = "libproxy_preload.so";
constexpr std::string_view kSocketVendor = "socket_vendor";
constexpr std::string_view kResolvConfPath = "/etc/resolv.conf";
constexpr std::string_view kResolvOverrideConfPath =
    "/etc/resolv_override.conf";
constexpr std::string_view kUseVcOption = "use-vc";

constexpr std::string_view kResolvConfContent = R"resolv(
; use-vc forces use of TCP for DNS resolutions.
; See https://man7.org/linux/man-pages/man5/resolv.conf.5.html
options use-vc timeout:2 attempts:5
search ec2.internal
; Ip of AWS DNS resolver located at base of VPC IPV4 network range, plus two.
; See https://docs.aws.amazon.com/vpc/latest/userguide/vpc-dns.html#AmazonDNS
nameserver 10.0.0.2
)resolv";
constexpr size_t kResolvConfContentLength = kResolvConfContent.length() - 1;

int main(int argc, char* argv[]) {
  std::string exe = argv[0];
  if (argc < 2) {
    std::cerr << "Usage: " << exe << " app_to_execute" << std::endl;
    return -1;
  }
  // Get current process's executable file path by reading link /proc/[pid]/exe
  int my_pid = getpid();
  std::string proc_exe_path =
      std::string("/proc/") + std::to_string(my_pid) + "/exe";
  auto my_path = std::make_unique<char[]>(PATH_MAX);
  if (ssize_t sz = readlink(proc_exe_path.c_str(), my_path.get(), PATH_MAX);
      sz < 0) {
    std::cerr << "ERROR: cannot access " << proc_exe_path << ": "
              << strerror(errno) << std::endl;
    return -1;
  }
  // Get the dir name of the executable.
  std::string dir_name = dirname(my_path.get());
  // Here we assume the preload lib is in the same directory.
  std::string lib_path = absl::StrCat(dir_name, "/", kLib.data());
  if (access(lib_path.c_str(), F_OK) != 0) {
    std::cerr << "ERROR: Cannot access " << lib_path << ": " << strerror(errno)
              << std::endl;
    return -1;
  }
  std::string socket_vendor_path =
      absl::StrCat(dir_name, "/", kSocketVendor.data());
  if (access(socket_vendor_path.c_str(), F_OK) != 0) {
    std::cerr << "ERROR: Cannot access " << socket_vendor_path << ": "
              << strerror(errno) << std::endl;
    return -1;
  }
  // Run the socket_vendor. If there's already a running socket_vendor, this
  // will end with a benign failure.
  if (fork() == 0) {
    daemon(1, 0);
    execl(socket_vendor_path.c_str(), socket_vendor_path.c_str(), nullptr);
    exit(1);
  }
  if (setenv("LD_PRELOAD", lib_path.c_str(), 1)) {
    std::cerr << "ERROR: cannot set LD_PRELOAD: " << strerror(errno)
              << std::endl;
    return -1;
  }
  if (setenv("RES_OPTIONS", kUseVcOption.data(), 1)) {
    std::cerr << "ERROR: cannot set RES_OPTIONS: " << strerror(errno)
              << std::endl;
    return -1;
  }
  // Before we execute, we need to make sure /etc/resolv.conf exists.
  if (access(kResolvConfPath.data(), F_OK) != 0) {
    // We cannot access /etc/resolv.conf, create one. resolv.conf may be a
    // dangling symlink, unlink it first.
    unlink(kResolvConfPath.data());
    // Create and open file for writing with mode 0644.
    int f = open(kResolvConfPath.data(), O_CREAT | O_EXCL | O_WRONLY,
                 S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    if (f < 0) {
      std::cerr << "ERROR: cannot open /etc/resolv.conf, and cannot create it."
                << std::endl;
      return -1;
    }
    // If the kResolvOverrideConfPath file does not exist, write to
    // kResolvConfPath. Else, create a symlink kResolvOverrideConfPath ->
    // kResolvConfPath overriding the kResolvConfPath contents.
    if (access(kResolvOverrideConfPath.data(), F_OK) != 0) {
      // Write the file, excluding the null terminator at end.
      if (const ssize_t r =
              write(f, kResolvConfContent.data(), kResolvConfContentLength);
          r != kResolvConfContentLength) {
        std::cerr << "ERROR: cannot fully write /etc/resolv.conf." << std::endl;
        return -1;
      }
    } else {
      if (remove(kResolvConfPath.data()) < 0) {
        std::cerr << "ERROR: could not remove /etc/resolv.conf." << std::endl;
        return -1;
      }
      if (link(kResolvOverrideConfPath.data(), kResolvConfPath.data()) < 0) {
        std::cerr << "ERROR: could not link /etc/resolv.conf." << std::endl;
        return -1;
      }
    }
    close(f);
  }
  // Execute!
  execvp(argv[1], &argv[1]);
  // If execution reaches here, above call has failed.
  std::cerr << "ERROR: cannot execute " << argv[1] << ": " << strerror(errno)
            << std::endl;
  return -1;
}
