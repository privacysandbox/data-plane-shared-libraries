// Copyright 2024 Google LLC
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

#include "src/communication/framing_utils.h"

#include "gtest/gtest.h"

namespace privacy_sandbox::server_common {
namespace {

TEST(FramingUtilsTest, EncodedDataSizeMatchesTheSpec) {
  size_t min_result_bytes = 0;
  EXPECT_EQ(GetEncodedDataSize(0, min_result_bytes), 8);
  EXPECT_EQ(GetEncodedDataSize(1, min_result_bytes), 8);
  EXPECT_EQ(GetEncodedDataSize(3, min_result_bytes), 8);
  EXPECT_EQ(GetEncodedDataSize(4, min_result_bytes), 16);
  EXPECT_EQ(GetEncodedDataSize(100, min_result_bytes), 128);
  EXPECT_EQ(GetEncodedDataSize(1000, min_result_bytes), 1024);
}

}  // namespace
}  // namespace privacy_sandbox::server_common
