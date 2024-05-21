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

#include "src/roma/benchmark/fake_kv_server.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <utility>

#include "src/roma/config/config.h"

using google::scp::roma::proto::FunctionBindingIoProto;
using ::testing::StrEq;

namespace google::scp::roma::test {

TEST(FakeKvServerTest, SmokeTest) {
  Config config;
  benchmark::FakeKvServer server(std::move(config));
}

TEST(FakeKvServerTest, ExecuteCodeWithNoKeyData) {
  Config config;
  benchmark::FakeKvServer server(std::move(config));

  benchmark::CodeConfig code_config;
  code_config.js = "hello = () => 'Hello world!';";
  code_config.udf_handler_name = "hello";
  server.SetCodeObject(code_config);

  EXPECT_THAT(server.ExecuteCode({}), StrEq(R"("Hello world!")"));
}

TEST(FakeKvServerTest, ExecuteCodeWithKeyData) {
  Config config;
  benchmark::FakeKvServer server(std::move(config));

  benchmark::CodeConfig code_config;
  code_config.js =
      "hello = (input) => 'Hello world! ' + JSON.stringify(input);";
  code_config.udf_handler_name = "hello";
  server.SetCodeObject(code_config);

  EXPECT_THAT(server.ExecuteCode({R"("ECHO")"}),
              StrEq(R"("Hello world! \"ECHO\"")"));
}

static void HelloWorldCallback(FunctionBindingPayload<>& wrapper) {
  wrapper.io_proto.set_output_string("I am a callback!");
}

TEST(FakeKvServerTest, ExecuteCodeWithCallback) {
  Config config;
  {
    auto function_object = std::make_unique<FunctionBindingObjectV2<>>();
    function_object->function_name = "callback";
    function_object->function = HelloWorldCallback;
    config.RegisterFunctionBinding(std::move(function_object));
  }
  benchmark::FakeKvServer server(std::move(config));

  benchmark::CodeConfig code_config;
  code_config.js = "hello = () => 'Hello world! ' + callback();";
  code_config.udf_handler_name = "hello";
  server.SetCodeObject(code_config);

  EXPECT_THAT(server.ExecuteCode({}),
              StrEq(R"("Hello world! I am a callback!")"));
}

}  // namespace google::scp::roma::test
