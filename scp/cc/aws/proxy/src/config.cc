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

#include "config.h"

#include <getopt.h>

#include <string_view>

#include "absl/strings/numbers.h"
#include "glog/logging.h"

namespace google::scp::proxy {

Config::Config()
    : buffer_size_(kDefaultBufferSize),
      socks5_port_(kDefaultPort),
      vsock_(true) {}

Config Config::Parse(const int argc, char* argv[]) {
  Config config;

  constexpr struct option long_options[]{
      {
          .name = "tcp",
          .has_arg = no_argument,
          .flag = 0,
          .val = 't',
      },
      {
          .name = "port",
          .has_arg = required_argument,
          .flag = 0,
          .val = 'p',
      },
      {
          .name = "buffer_size",
          .has_arg = required_argument,
          .flag = 0,
          .val = 'b',
      },
      {
          .name = nullptr,
          .has_arg = 0,
          .flag = 0,
          .val = 0,
      },
  };

  while (true) {
    int opt_idx = 0;
    const int c = getopt_long(argc, argv, "tp:b:", long_options, &opt_idx);
    if (c == -1) {
      break;
    }
    switch (c) {
      case 0: {
        break;
      }
      case 't': {
        config.vsock_ = false;
        break;
      }
      case 'p': {
        char* endptr;
        if (optarg == nullptr) {
          LOG(FATAL) << "ERROR: Port value must be specified for -p flag";
          exit(1);
        }
        const std::string_view port_str(optarg);
        uint64_t port;
        if (!absl::SimpleAtoi(port_str, &port)) {
          LOG(FATAL) << "ERROR: Unable to parse port number: " << port_str;
          exit(1);
        }
        if (port > UINT16_MAX) {
          LOG(FATAL) << "ERROR: Invalid port number: " << port_str;
        }
        config.socks5_port_ = static_cast<uint16_t>(port);
        break;
      }
      case 'b': {
        char* endptr;
        if (optarg == nullptr) {
          LOG(FATAL) << "ERROR: Buffer size must be specified for -b flag";
          exit(1);
        }
        const std::string_view bs_str(optarg);
        uint64_t bs;
        if (!absl::SimpleAtoi(bs_str, &bs) || bs == 0) {
          LOG(FATAL) << "ERROR: Invalid buffer size: " << bs_str;
          exit(1);
        }
        config.buffer_size_ = bs;
        break;
      }
      case '?': {
        LOG(FATAL) << "ERROR: unrecognized command-line option.";
      }
      default: {
        LOG(FATAL) << "ERROR: unknown error.";
      }
    }
  }
  return config;
}
}  // namespace google::scp::proxy
