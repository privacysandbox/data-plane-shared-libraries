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

#include "proxy/src/config.h"

#include <gtest/gtest.h>

namespace google::scp::proxy {

namespace test {

TEST(ConfigTest, MissingRequiredArgs) {
  char* argv[] = {"program_name", nullptr};
  constexpr int argc = 2;
  EXPECT_DEATH(Config::Parse(argc, argv), /*Error regex*/ "");
}

TEST(ConfigTest, PortNumberNotNumeric) {
  char* argv[] = {"program_name", "-p", "1a", "-b", "1", nullptr};
  constexpr int argc = 6;
  EXPECT_DEATH(Config::Parse(argc, argv),
               "ERROR: Unable to parse port number:.*");
}

TEST(ConfigTest, PortNumberTooHigh) {
  char* argv[] = {"program_name", "-p", "100000", "-b", "1", nullptr};
  constexpr int argc = 6;
  EXPECT_DEATH(Config::Parse(argc, argv), "ERROR: Invalid port number:.*");
}

TEST(ConfigTest, PortMissingArgValue) {
  char* argv[] = {"program_name", "-p", nullptr};
  constexpr int argc = 6;
  EXPECT_DEATH(Config::Parse(argc, argv),
               "ERROR: Port value must be specified for -p flag");
}

TEST(ConfigTest, BadBufferSize) {
  char* argv[] = {"program_name", "-p", "1", "-b", "notanumber", nullptr};
  constexpr int argc = 6;
  EXPECT_DEATH(Config::Parse(argc, argv), "ERROR: Invalid buffer size: .*");
}

TEST(ConfigTest, BufferMissingValue) {
  char* argv[] = {"program_name", "-b", nullptr};
  constexpr int argc = 3;
  EXPECT_DEATH(Config::Parse(argc, argv),
               "ERROR: Buffer size must be specified for -b flag");
}

TEST(ConfigTest, UnexpectedArg) {
  char* argv[] = {"program_name", "-b", "1", "-p", "0", "-x", nullptr};
  constexpr int argc = 6;
  EXPECT_DEATH(Config::Parse(argc, argv),
               "ERROR: unrecognized command-line option.");
}

TEST(ConfigTest, Success) {
  char* argv[] = {"program_name", "-p", "0", "-b", "1", nullptr};
  constexpr int argc = 5;
  const Config config = Config::Parse(argc, argv);
  EXPECT_EQ(config.socks5_port_, 0);
  EXPECT_EQ(config.buffer_size_, 1);
  EXPECT_TRUE(config.vsock_);
}

TEST(ConfigTest, Tcp) {
  char* argv[] = {"program_name", "-p", "0", "-b", "1", "-t", nullptr};
  constexpr int argc = 6;
  const Config config = Config::Parse(argc, argv);
  EXPECT_FALSE(config.vsock_);
}

}  // namespace test
}  // namespace google::scp::proxy
