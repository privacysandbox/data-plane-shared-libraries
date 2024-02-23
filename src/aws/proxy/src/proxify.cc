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

#include <array>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

#include "absl/flags/parse.h"
#include "absl/strings/str_cat.h"

namespace {

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
nameserver 10.0.0.2)resolv";

}  // namespace

int main(int argc, char* argv[]) {
  std::vector<char*> positional_args = absl::ParseCommandLine(argc, argv);
  if (positional_args.size() < 2) {
    std::cerr << "Usage: proxify -- <app> [<app flag>...]" << std::endl;
    return -1;
  }

  // Get current process's executable file path by reading link /proc/[pid]/exe.
  std::array<char, PATH_MAX> proxify_path;
  if (const std::string link_path = absl::StrCat("/proc/", getpid(), "/exe");
      readlink(link_path.c_str(), proxify_path.data(), PATH_MAX) < 0) {
    std::cerr << "ERROR: cannot access " << link_path << ": " << strerror(errno)
              << std::endl;
    return -1;
  }
  const std::string_view proxify_dir = dirname(proxify_path.data());

  // Assume the preload lib is in the same directory.
  if (const std::string socket_vendor_path =
          absl::StrCat(proxify_dir, "/", kSocketVendor);
      access(socket_vendor_path.c_str(), F_OK) != 0) {
    std::cerr << "ERROR: Cannot access " << socket_vendor_path << ": "
              << strerror(errno) << std::endl;
    return -1;
  } else if (fork() == 0) {
    // Fork and run the socket_vendor. If there's already a running
    // socket_vendor, this will end with a benign failure.
    (void)daemon(1, 0);
    execl(socket_vendor_path.c_str(), socket_vendor_path.c_str(), nullptr);
    return -1;
  }
  if (const std::string lib_path = absl::StrCat(proxify_dir, "/", kLib);
      access(lib_path.c_str(), F_OK) != 0) {
    std::cerr << "ERROR: Cannot access " << lib_path << ": " << strerror(errno)
              << std::endl;
    return -1;
  } else if (setenv("LD_PRELOAD", lib_path.c_str(), 1)) {
    std::cerr << "ERROR: cannot set LD_PRELOAD: " << strerror(errno)
              << std::endl;
    return -1;
  }
  if (setenv("RES_OPTIONS", kUseVcOption.data(), 1)) {
    std::cerr << "ERROR: cannot set RES_OPTIONS: " << strerror(errno)
              << std::endl;
    return -1;
  }

  // Ensure /etc/resolv.conf exists.
  if (access(kResolvConfPath.data(), F_OK) != 0) {
    // We cannot access /etc/resolv.conf, create one. resolv.conf may be a
    // dangling symlink, unlink it first.
    unlink(kResolvConfPath.data());

    // Create and open file for writing with mode 0644.
    const int f = open(kResolvConfPath.data(), O_CREAT | O_EXCL | O_WRONLY,
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
      if (write(f, kResolvConfContent.data(), kResolvConfContent.size()) ==
          -1) {
        std::cerr << "ERROR: cannot write /etc/resolv.conf." << std::endl;
        return -1;
      }
    } else if (remove(kResolvConfPath.data()) < 0) {
      std::cerr << "ERROR: could not remove /etc/resolv.conf." << std::endl;
      return -1;
    } else if (link(kResolvOverrideConfPath.data(), kResolvConfPath.data()) <
               0) {
      std::cerr << "ERROR: could not link /etc/resolv.conf." << std::endl;
      return -1;
    }
    close(f);
  }

  // The second arg to `execvp` must be null terminated.
  positional_args.push_back(nullptr);

  // Execute!
  execvp(positional_args[1], &positional_args[1]);

  // If execution reaches here, above call has failed.
  std::cerr << "ERROR: cannot execute " << positional_args[1] << ": "
            << strerror(errno) << std::endl;
  return -1;
}
