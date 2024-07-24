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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/config/config.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

using google::scp::roma::FunctionBindingPayload;
using google::scp::roma::sandbox::roma_service::RomaService;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::StrEq;

namespace google::scp::roma::test {

namespace {
std::unique_ptr<FunctionBindingObjectV2<>> CreateFunctionBindingObjectV2(
    std::string_view function_name,
    std::function<void(FunctionBindingPayload<>&)> function) {
  return std::make_unique<FunctionBindingObjectV2<>>(FunctionBindingObjectV2<>{
      .function_name = std::string(function_name),
      .function = function,
  });
}
}  // namespace

void StringInStringOutFunction(FunctionBindingPayload<>& wrapper) {
  wrapper.io_proto.set_output_string(wrapper.io_proto.input_string() +
                                     " String from C++");
}

TEST(FunctionBindingTest,
     CanRegisterBindingAndExecuteCodeThatCallsItWithInputAndOutputString) {
  RomaService<>::Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(CreateFunctionBindingObjectV2(
      "cool_function", StringInStringOutFunction));

  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
          function Handler(input) { return cool_function(input);}
    )JS_CODE",
    });

    absl::Status response;
    EXPECT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    EXPECT_TRUE(response.ok());
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("Foobar")"},
        });

    absl::Status response;
    EXPECT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response = resp.status();
                               if (resp.ok()) {
                                 result = std::move(resp->resp);
                               }
                               execute_finished.Notify();
                             })
                    .ok());
    EXPECT_TRUE(response.ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_THAT(result, StrEq(R"("Foobar String from C++")"));

  EXPECT_TRUE(roma_service.Stop().ok());
}

void ListOfStringInListOfStringOutFunction(FunctionBindingPayload<>& wrapper) {
  int i = 1;

  for (auto& str : wrapper.io_proto.input_list_of_string().data()) {
    wrapper.io_proto.mutable_output_list_of_string()->mutable_data()->Add(
        str + " Some other stuff " + std::to_string(i++));
  }
}

TEST(
    FunctionBindingTest,
    CanRegisterBindingAndExecuteCodeThatCallsItWithInputAndOutputListOfString) {
  RomaService<>::Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(CreateFunctionBindingObjectV2(
      "cool_function", ListOfStringInListOfStringOutFunction));

  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
          function Handler() {
            some_array = ["str 1", "str 2", "str 3"];
            return cool_function(some_array);
          }
    )JS_CODE",
    });

    absl::Status response;
    EXPECT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    EXPECT_TRUE(response.ok());
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
        });

    absl::Status response;
    EXPECT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response = resp.status();
                               if (resp.ok()) {
                                 result = std::move(resp->resp);
                               }
                               execute_finished.Notify();
                             })
                    .ok());
    EXPECT_TRUE(response.ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_THAT(
      result,
      StrEq(
          R"(["str 1 Some other stuff 1","str 2 Some other stuff 2","str 3 Some other stuff 3"])"));

  EXPECT_TRUE(roma_service.Stop().ok());
}

void MapOfStringInMapOfStringOutFunction(FunctionBindingPayload<>& wrapper) {
  for (auto& [key, value] : wrapper.io_proto.input_map_of_string().data()) {
    std::string new_key;
    std::string new_val;
    if (key == "key-a") {
      new_key = key + std::to_string(1);
      new_val = value + std::to_string(1);
    } else {
      new_key = key + std::to_string(2);
      new_val = value + std::to_string(2);
    }
    (*wrapper.io_proto.mutable_output_map_of_string()
          ->mutable_data())[new_key] = new_val;
  }
}

TEST(FunctionBindingTest,
     CanRegisterBindingAndExecuteCodeThatCallsItWithInputAndOutputMapOfString) {
  RomaService<>::Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(CreateFunctionBindingObjectV2(
      "cool_function", MapOfStringInMapOfStringOutFunction));

  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
          function Handler() {
            some_map = [["key-a","value-a"], ["key-b","value-b"]];
            // Since we can't stringify a Map, we build an array from the resulting map entries.
            returned_map = cool_function(new Map(some_map));
            return Array.from(returned_map.entries());
          }
    )JS_CODE",
    });

    absl::Status response;
    EXPECT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    EXPECT_TRUE(response.ok());
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
        });

    absl::Status response;
    EXPECT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response = resp.status();
                               if (resp.ok()) {
                                 result = std::move(resp->resp);
                               }
                               execute_finished.Notify();
                             })
                    .ok());
    EXPECT_TRUE(response.ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  // Since the map makes it over the wire, we can't guarantee the order of the
  // keys so we assert that the expected key-value pairs are present.
  EXPECT_THAT(result, HasSubstr(R"(["key-a1","value-a1"])"));
  EXPECT_THAT(result, HasSubstr(R"(["key-b2","value-b2"])"));

  EXPECT_TRUE(roma_service.Stop().ok());
}

void StringInStringOutFunctionWithNoInputParams(
    FunctionBindingPayload<>& wrapper) {
  // Params are all empty
  EXPECT_FALSE(wrapper.io_proto.has_input_string());
  EXPECT_FALSE(wrapper.io_proto.has_input_list_of_string());
  EXPECT_FALSE(wrapper.io_proto.has_input_map_of_string());

  wrapper.io_proto.set_output_string("String from C++");
}

TEST(FunctionBindingTest, CanCallFunctionBindingThatDoesNotTakeAnyArguments) {
  RomaService<>::Config config;
  config.number_of_workers = 2;

  config.RegisterFunctionBinding(CreateFunctionBindingObjectV2(
      "cool_function", StringInStringOutFunctionWithNoInputParams));

  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
          function Handler() { return cool_function();}
    )JS_CODE",
    });

    absl::Status response;
    EXPECT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    EXPECT_TRUE(response.ok());
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
        });

    absl::Status response;
    EXPECT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response = resp.status();
                               if (resp.ok()) {
                                 result = std::move(resp->resp);
                               }
                               execute_finished.Notify();
                             })
                    .ok());
    EXPECT_TRUE(response.ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));

  EXPECT_THAT(result, StrEq(R"("String from C++")"));

  EXPECT_TRUE(roma_service.Stop().ok());
}

void ByteOutFunction(FunctionBindingPayload<>& wrapper) {
  const std::vector<uint8_t> data = {1, 2, 3, 4, 4, 3, 2, 1};
  wrapper.io_proto.set_output_bytes(data.data(), data.size());
}

TEST(FunctionBindingTest,
     CanRegisterBindingAndExecuteCodeThatCallsItWithOutputBytes) {
  RomaService<>::Config config;
  config.number_of_workers = 2;

  config.RegisterFunctionBinding(
      CreateFunctionBindingObjectV2("get_some_bytes", ByteOutFunction));

  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
          function Handler() {
            bytes = get_some_bytes();
            if (bytes instanceof Uint8Array) {
              return bytes;
            }

            return "Didn't work :(";
          }
    )JS_CODE",
    });

    absl::Status response;
    EXPECT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    EXPECT_TRUE(response.ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
        });

    absl::Status response;
    EXPECT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response = resp.status();
                               result = std::move(resp->resp);
                               execute_finished.Notify();
                             })
                    .ok());
    EXPECT_TRUE(response.ok());
  }
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));

  EXPECT_THAT(result,
              StrEq(R"({"0":1,"1":2,"2":3,"3":4,"4":4,"5":3,"6":2,"7":1})"));

  EXPECT_TRUE(roma_service.Stop().ok());
}

void ByteInFunction(FunctionBindingPayload<>& wrapper) {
  auto data_len = wrapper.io_proto.input_bytes().size();
  EXPECT_EQ(5, data_len);
  auto byte_data = wrapper.io_proto.input_bytes().data();
  EXPECT_EQ(5, byte_data[0]);
  EXPECT_EQ(4, byte_data[1]);
  EXPECT_EQ(3, byte_data[2]);
  EXPECT_EQ(2, byte_data[3]);
  EXPECT_EQ(1, byte_data[4]);

  wrapper.io_proto.set_output_string("Hello there :)");
}

TEST(FunctionBindingTest,
     CanRegisterBindingAndExecuteCodeThatCallsItWithInputBytes) {
  RomaService<>::Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(
      CreateFunctionBindingObjectV2("set_some_bytes", ByteInFunction));

  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
          function Handler() {
            bytes =  new Uint8Array(5);
            bytes[0] = 5;
            bytes[1] = 4;
            bytes[2] = 3;
            bytes[3] = 2;
            bytes[4] = 1;

            return set_some_bytes(bytes);
          }
    )JS_CODE",
    });

    absl::Status response;
    EXPECT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    EXPECT_TRUE(response.ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
        });

    absl::Status response;
    EXPECT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response = resp.status();
                               result = std::move(resp->resp);
                               execute_finished.Notify();
                             })
                    .ok());
    EXPECT_TRUE(response.ok());
  }
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));

  EXPECT_THAT(result, StrEq(R"str("Hello there :)")str"));

  EXPECT_TRUE(roma_service.Stop().ok());
}

void FooBarFunc1(FunctionBindingPayload<>& wrapper) {
  wrapper.io_proto.set_output_string(wrapper.io_proto.input_string() +
                                     " String from foo.bar.func1");
}

void FoObarFunc2(FunctionBindingPayload<>& wrapper) {
  wrapper.io_proto.set_output_string(wrapper.io_proto.input_string() +
                                     " String from fo.obar.func2");
}

TEST(FunctionBindingTest, CanRegisterNonGlobalBindingsAndExecuteCode) {
  RomaService<>::Config config;
  config.number_of_workers = 3;
  config.RegisterFunctionBinding(
      CreateFunctionBindingObjectV2("foo.bar.func1", FooBarFunc1));
  config.RegisterFunctionBinding(
      CreateFunctionBindingObjectV2("fo.obar.func2", FoObarFunc2));

  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = R"JS_CODE(
          function Handler(input) {
            return `func1: ${foo.bar.func1(input)} func2: ${fo.obar.func2(input)}`;
          })JS_CODE",
    });

    absl::Status response;
    EXPECT_TRUE(roma_service
                    .LoadCodeObj(std::move(code_obj),
                                 [&](absl::StatusOr<ResponseObject> resp) {
                                   response = resp.status();
                                   load_finished.Notify();
                                 })
                    .ok());
    EXPECT_TRUE(response.ok());
  }

  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {R"("Foobar")"},
        });

    absl::Status response;
    EXPECT_TRUE(roma_service
                    .Execute(std::move(execution_obj),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               response = resp.status();
                               if (resp.ok()) {
                                 result = std::move(resp->resp);
                               }
                               execute_finished.Notify();
                             })
                    .ok());
    EXPECT_TRUE(response.ok());
  }
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10)));
  EXPECT_THAT(
      result,
      StrEq(
          R"("func1: Foobar String from foo.bar.func1 func2: Foobar String from fo.obar.func2")"));

  EXPECT_TRUE(roma_service.Stop().ok());
}

}  // namespace google::scp::roma::test
