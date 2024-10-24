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

#include "src/roma/byob/tools/shell_evaluator.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fstream>
#include <istream>
#include <string>
#include <string_view>

#include "absl/status/statusor.h"

namespace privacy_sandbox::server_common::byob {
namespace {
using ::testing::HasSubstr;

absl::StatusOr<std::string> TrivialLoadFn(std::string_view /*udf*/) {
  return "some_code_token";
}
absl::StatusOr<std::string> TrivialExecuteFn(
    std::string_view /*rpc*/, std::string_view /*code_token*/,
    std::string_view /*request_json*/) {
  return "some_serialized_response";
}

TEST(ShellEvaluatorTest, ContinueWithEmptyOrWhitespaceInputLine) {
  ShellEvaluator evaluator(/*service_specific_message=*/"", /*rpcs=*/{},
                           TrivialLoadFn, TrivialExecuteFn);
  EXPECT_EQ(evaluator.EvalAndPrint("", /*disable_commands=*/true,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kContinue);
  EXPECT_EQ(evaluator.EvalAndPrint(" \t\n", /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kContinue);
}

TEST(ShellEvaluatorTest, ExitWhenInputSaysSo) {
  ShellEvaluator evaluator(/*service_specific_message=*/"", /*rpcs=*/{},
                           TrivialLoadFn, TrivialExecuteFn);
  EXPECT_EQ(evaluator.EvalAndPrint("exit", /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kExit);
}

TEST(ShellEvaluatorTest, ErrorWhenCommandsPassedNoArguments) {
  ShellEvaluator evaluator(/*service_specific_message=*/"", /*rpcs=*/{},
                           TrivialLoadFn, TrivialExecuteFn);
  EXPECT_EQ(evaluator.EvalAndPrint("commands", /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kError);
}

TEST(ShellEvaluatorTest, ErrorWhenCommandsFileDoesntExist) {
  ShellEvaluator evaluator(/*service_specific_message=*/"", /*rpcs=*/{},
                           TrivialLoadFn, TrivialExecuteFn);
  EXPECT_EQ(evaluator.EvalAndPrint("commands file_that_doesnt_exist.txt",
                                   /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kError);
}

TEST(ShellEvaluatorTest, ContinueWhenCommandsPassedEmptyFile) {
  ShellEvaluator evaluator(/*service_specific_message=*/"", /*rpcs=*/{},
                           TrivialLoadFn, TrivialExecuteFn);
  EXPECT_EQ(
      evaluator.EvalAndPrint("commands /dev/null", /*disable_commands=*/false,
                             /*print_response=*/true),
      ShellEvaluator::NextStep::kContinue);
}

TEST(ShellEvaluatorTest, ErrorWhenInputSaysCommandsAndCommandsIsDisabled) {
  ShellEvaluator evaluator(/*service_specific_message=*/"", /*rpcs=*/{},
                           TrivialLoadFn, TrivialExecuteFn);
  EXPECT_EQ(
      evaluator.EvalAndPrint("commands /dev/null", /*disable_commands=*/true,
                             /*print_response=*/true),
      ShellEvaluator::NextStep::kError);
}

TEST(ShellEvaluatorTest, ErrorWhenCommandsFileContainsCommands) {
  ShellEvaluator evaluator(/*service_specific_message=*/"", /*rpcs=*/{},
                           TrivialLoadFn, TrivialExecuteFn);
  {
    std::ofstream ofs("commands.txt");
    ofs << "commands\n";
  }
  EXPECT_EQ(evaluator.EvalAndPrint("commands commands.txt",
                                   /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kError);
}

TEST(ShellEvaluatorTest, ExitWhenCommandsFileContainsExit) {
  ShellEvaluator evaluator(/*service_specific_message=*/"", /*rpcs=*/{},
                           TrivialLoadFn, TrivialExecuteFn);
  {
    std::ofstream ofs("exit.txt");
    ofs << "exit\n";
  }
  EXPECT_EQ(evaluator.EvalAndPrint("commands exit.txt",
                                   /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kExit);
}

TEST(ShellEvaluatorTest, ContinueAfterHelpCall) {
  ShellEvaluator evaluator(/*service_specific_message=*/"", /*rpcs=*/{},
                           TrivialLoadFn, TrivialExecuteFn);
  EXPECT_EQ(evaluator.EvalAndPrint("help", /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kContinue);
}

TEST(ShellEvaluatorTest, ErrorWhenLoadMissingArguments) {
  ShellEvaluator evaluator(/*service_specific_message=*/"", {"rpc_1"},
                           TrivialLoadFn, TrivialExecuteFn);
  EXPECT_EQ(evaluator.EvalAndPrint("load", /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kError);
  EXPECT_EQ(evaluator.EvalAndPrint("load rpc_1", /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kError);
}

TEST(ShellEvaluatorTest, ErrorWhenLoadingUnrecognizedRpc) {
  ShellEvaluator evaluator(/*service_specific_message=*/"", /*rpcs=*/{},
                           TrivialLoadFn, TrivialExecuteFn);
  EXPECT_EQ(evaluator.EvalAndPrint("load some_rpc /dev/null",
                                   /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kError);
}

TEST(ShellEvaluatorTest, ErrorWhenCommandUnrecognized) {
  ShellEvaluator evaluator(/*service_specific_message=*/"", /*rpcs=*/{},
                           TrivialLoadFn, TrivialExecuteFn);
  EXPECT_EQ(evaluator.EvalAndPrint("unregistered_rpc",
                                   /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kError);
}
TEST(ShellEvaluatorTest, ErrorWhenExecBeforeLoad) {
  ShellEvaluator evaluator(/*service_specific_message=*/"", {"rpc_1"},
                           TrivialLoadFn, TrivialExecuteFn);
  EXPECT_EQ(evaluator.EvalAndPrint("rpc_1 /dev/null",
                                   /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kError);
}

TEST(ShellEvaluatorTest, ErrorWhenExecWrongNumberOfArgs) {
  ShellEvaluator evaluator(/*service_specific_message=*/"", {"rpc_1"},
                           TrivialLoadFn, TrivialExecuteFn);
  EXPECT_EQ(evaluator.EvalAndPrint("load rpc_1 /dev/null",
                                   /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kContinue);
  EXPECT_EQ(evaluator.EvalAndPrint("rpc_1", /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kError);
}

TEST(ShellEvaluatorTest, ErrorWhenExecRequestFileNotFound) {
  ShellEvaluator evaluator(/*service_specific_message=*/"", {"rpc_1"},
                           TrivialLoadFn, TrivialExecuteFn);
  EXPECT_EQ(evaluator.EvalAndPrint("load rpc_1 /dev/null",
                                   /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kContinue);
  EXPECT_EQ(evaluator.EvalAndPrint("rpc_1 nonexistant_file.txt",
                                   /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kError);
}

TEST(ShellEvaluatorTest, ExecAppendsToResponseFile) {
  auto execute_fn = [](auto, auto, auto) {
    return "my_super_special_response";
  };
  ShellEvaluator evaluator(/*service_specific_message=*/"", {"rpc_1"},
                           TrivialLoadFn, execute_fn);
  EXPECT_EQ(evaluator.EvalAndPrint("load rpc_1 /dev/null",
                                   /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kContinue);
  EXPECT_EQ(evaluator.EvalAndPrint("rpc_1 /dev/null response.txt",
                                   /*disable_commands=*/false,
                                   /*print_response=*/true),
            ShellEvaluator::NextStep::kContinue);
  std::ifstream ifs("response.txt");
  EXPECT_THAT(std::string(std::istreambuf_iterator<char>(ifs),
                          std::istreambuf_iterator<char>()),
              HasSubstr("my_super_special_response"));
}
}  // namespace
}  // namespace privacy_sandbox::server_common::byob
