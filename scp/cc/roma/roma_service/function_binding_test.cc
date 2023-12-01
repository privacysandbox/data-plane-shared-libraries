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
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/log/scoped_mock_log.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "roma/config/src/config.h"
#include "roma/config/src/function_binding_object_v2.h"
#include "roma/interface/roma.h"
#include "roma/roma_service/roma_service.h"

using google::scp::roma::FunctionBindingPayload;
using google::scp::roma::sandbox::roma_service::RomaService;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::StrEq;

namespace google::scp::roma::test {

TEST(FunctionBindingTest, ExecuteNativeLogFunctions) {
  Config config;
  config.number_of_workers = 2;
  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  const auto& input = R"("Foobar")";
  const auto& trim_first_last_char = [](const std::string& str) {
    return str.substr(1, str.length() - 2);
  };

  absl::ScopedMockLog log;
  EXPECT_CALL(log,
              Log(absl::LogSeverity::kInfo, _, trim_first_last_char(input)));
  EXPECT_CALL(log,
              Log(absl::LogSeverity::kWarning, _, trim_first_last_char(input)));
  EXPECT_CALL(log,
              Log(absl::LogSeverity::kError, _, trim_first_last_char(input)));
  log.StartCapturingLogs();

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) {
      roma.n_log(input);
      roma.n_warn(input);
      roma.n_error(input);
      return `Hello world! ${input}`;
    }
  )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(input);

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result,
              testing::StrEq(absl::StrCat(R"("Hello world! )",
                                          trim_first_last_char(input), "\"")));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());

  log.StopCapturingLogs();
}

void StringInStringOutFunction(FunctionBindingPayload<>& wrapper) {
  wrapper.io_proto.set_output_string(wrapper.io_proto.input_string() +
                                     " String from C++");
}

TEST(FunctionBindingTest,
     CanRegisterBindingAndExecuteCodeThatCallsItWithInputAndOutputString) {
  Config config;
  config.number_of_workers = 2;
  auto function_binding_object = std::make_unique<FunctionBindingObjectV2<>>();
  function_binding_object->function = StringInStringOutFunction;
  function_binding_object->function_name = "cool_function";
  config.RegisterFunctionBinding(std::move(function_binding_object));

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return cool_function(input);}
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(R"("Foobar")");

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(result, StrEq(R"("Foobar String from C++")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
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
  Config config;
  config.number_of_workers = 2;
  auto function_binding_object = std::make_unique<FunctionBindingObjectV2<>>();
  function_binding_object->function = ListOfStringInListOfStringOutFunction;
  function_binding_object->function_name = "cool_function";
  config.RegisterFunctionBinding(std::move(function_binding_object));

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler() { some_array = ["str 1", "str 2", "str 3"]; return cool_function(some_array);}
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(
      result,
      StrEq(
          R"(["str 1 Some other stuff 1","str 2 Some other stuff 2","str 3 Some other stuff 3"])"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
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
  Config config;
  config.number_of_workers = 2;
  auto function_binding_object = std::make_unique<FunctionBindingObjectV2<>>();
  function_binding_object->function = MapOfStringInMapOfStringOutFunction;
  function_binding_object->function_name = "cool_function";
  config.RegisterFunctionBinding(std::move(function_binding_object));

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler() {
      some_map = [["key-a","value-a"], ["key-b","value-b"]];
      // Since we can't stringify a Map, we build an array from the resulting map entries.
      returned_map = cool_function(new Map(some_map));
      return Array.from(returned_map.entries());
    }
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  // Since the map makes it over the wire, we can't guarantee the order of the
  // keys so we assert that the expected key-value pairs are present.
  EXPECT_THAT(result, HasSubstr(R"(["key-a1","value-a1"])"));
  EXPECT_THAT(result, HasSubstr(R"(["key-b2","value-b2"])"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
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
  Config config;
  config.number_of_workers = 2;
  auto function_binding_object = std::make_unique<FunctionBindingObjectV2<>>();
  function_binding_object->function =
      StringInStringOutFunctionWithNoInputParams;
  function_binding_object->function_name = "cool_function";
  config.RegisterFunctionBinding(std::move(function_binding_object));

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler() { return cool_function();}
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          if (resp->ok()) {
            auto& code_resp = **resp;
            result = code_resp.resp;
          }
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  EXPECT_THAT(result, StrEq(R"("String from C++")"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

void ByteOutFunction(FunctionBindingPayload<>& wrapper) {
  const std::vector<uint8_t> data = {1, 2, 3, 4, 4, 3, 2, 1};
  wrapper.io_proto.set_output_bytes(data.data(), data.size());
}

TEST(FunctionBindingTest,
     CanRegisterBindingAndExecuteCodeThatCallsItWithOutputBytes) {
  Config config;
  config.number_of_workers = 2;
  auto function_binding_object = std::make_unique<FunctionBindingObjectV2<>>();
  function_binding_object->function = ByteOutFunction;
  function_binding_object->function_name = "get_some_bytes";
  config.RegisterFunctionBinding(std::move(function_binding_object));

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler() {
      bytes = get_some_bytes();
      if (bytes instanceof Uint8Array) {
        return bytes;
      }

      return "Didn't work :(";
    }
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          auto& code_resp = **resp;
          result = code_resp.resp;
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  EXPECT_THAT(result,
              StrEq(R"({"0":1,"1":2,"2":3,"3":4,"4":4,"5":3,"6":2,"7":1})"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
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
  Config config;
  config.number_of_workers = 2;
  auto function_binding_object = std::make_unique<FunctionBindingObjectV2<>>();
  function_binding_object->function = ByteInFunction;
  function_binding_object->function_name = "set_some_bytes";
  config.RegisterFunctionBinding(std::move(function_binding_object));

  auto roma_service = std::make_unique<RomaService<>>(config);
  auto status = roma_service->Init();
  EXPECT_TRUE(status.ok());

  std::string result;
  absl::Notification load_finished;
  absl::Notification execute_finished;

  {
    auto code_obj = std::make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler() {
      bytes =  new Uint8Array(5);
      bytes[0] = 5;
      bytes[1] = 4;
      bytes[2] = 3;
      bytes[3] = 2;
      bytes[4] = 1;

      return set_some_bytes(bytes);
    }
    )JS_CODE";

    status = roma_service->LoadCodeObj(
        std::move(code_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          load_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  {
    auto execution_obj = std::make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";

    status = roma_service->Execute(
        std::move(execution_obj),
        [&](std::unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          auto& code_resp = **resp;
          result = code_resp.resp;
          execute_finished.Notify();
        });
    EXPECT_TRUE(status.ok());
  }
  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  EXPECT_THAT(result, StrEq(R"str("Hello there :)")str"));

  status = roma_service->Stop();
  EXPECT_TRUE(status.ok());
}

}  // namespace google::scp::roma::test
