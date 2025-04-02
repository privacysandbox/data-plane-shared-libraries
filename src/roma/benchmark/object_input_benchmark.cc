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

#include <algorithm>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>
#include <nlohmann/json.hpp>

#include "absl/log/check.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/notification.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

using google::scp::roma::CodeObject;
using google::scp::roma::Config;
using google::scp::roma::FunctionBindingPayload;
using google::scp::roma::InvocationStrRequest;
using google::scp::roma::ResponseObject;
using google::scp::roma::sandbox::roma_service::RomaService;

namespace {

std::string MakeLabel(std::string name) {
  nlohmann::ordered_json json;
  json["perfgate_label"] = name;
  return json.dump();
}

constexpr absl::Duration kTimeout = absl::Seconds(10);

std::unique_ptr<RomaService<>> roma_service;

void StringInFunction(FunctionBindingPayload<>& wrapper) {
  std::string serialized_obj = wrapper.io_proto.input_string();
  nlohmann::json result_json = nlohmann::json::parse(serialized_obj);
}

void StructInFunction(FunctionBindingPayload<>& wrapper) {}

// Generates a JS object with 2*size keys.
std::string GetObject(int size) {
  int num_ads = size / 2;
  int num_ids = 100;

  std::string ads = "[";
  for (int i = 0; i < num_ads; i++) {
    std::string ids = "[";
    for (int j = 0; j < num_ids; j++) {
      absl::StrAppend(&ids, "'deal", j, "'");
      if (j < num_ids - 1) {
        absl::StrAppend(&ids, ",");
      }
    }
    ids += "]";

    std::string metadata(2000, 'a');

    std::string ad = absl::Substitute(
        R"({renderUrl: 'https://cdn.com/render_url_of_bid_$0', sizeGroup: 'group2', selectableBuyerAndSellerReportingIds: $1, buyerReportingId: 'buyerSpecificInfo1', buyerAndSellerReportingId: 'seatId', metadata: '$2'})",
        i, ids, metadata);

    absl::StrAppend(&ads, ad);
    if (i < num_ads - 1) {
      absl::StrAppend(&ads, ",");
    }
  }
  ads += "]";

  std::string ad_components = "[";
  for (int i = 0; i < num_ads; i++) {
    std::string ad_component = absl::Substitute(
        R"({renderUrl: 'https://cdn.com/render_url_of_bid_$0', sizeGroup: 'group2'})",
        i);
    absl::StrAppend(&ad_components, ad_component);
    if (i < num_ads - 1) {
      absl::StrAppend(&ad_components, ",");
    }
  }
  ad_components += "]";

  return absl::Substitute(R"({
  'ads': $0,
  'adComponents': $1,
  'adSizes':
      {'size1': {width: '100', height: '100'},
 'size2': {width: '100', height: '200'},
 'size3': {width: '75', height: '25'},
 'size4': {width: '100', height: '25'}},
  'sizeGroups' :
{'group1': ['size1', 'size2', 'size3'],
      'group2': ['size3', 'size4']},
  'biddingLogicURL': 'https://www.example-dsp.com/bid_logic.js'
})",
                          ads, ad_components);
}

void DoSetup(const benchmark::State& state) {
  Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(std::make_unique<FunctionBindingObjectV2<>>(
      FunctionBindingObjectV2<>{.function_name = "serialized_obj_callback",
                                .function = StringInFunction}));
  config.RegisterFunctionBinding(std::make_unique<FunctionBindingObjectV2<>>(
      FunctionBindingObjectV2<>{.function_name = "struct_obj_callback",
                                .function = StructInFunction}));
  roma_service = std::make_unique<RomaService<>>(std::move(config));
  CHECK_OK(roma_service->Init());
}

void DoTeardown(const benchmark::State& state) {
  CHECK_OK(roma_service->Stop());
}

void BM_ObjectAsSerializedStringInput(::benchmark::State& state) {
  state.SetLabel(MakeLabel(absl::StrCat(state.name(), "_", state.range(0))));
  absl::Notification load_finished;
  {
    std::string js_code = absl::Substitute(R"""(
      let obj = $0;
      function Handler() {
        return serialized_obj_callback(JSON.stringify(obj));
      }
        )""",
                                           GetObject(state.range(0)));

    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = js_code,
    });
    CHECK_OK(roma_service->LoadCodeObj(
        std::move(code_obj), [&](absl::StatusOr<ResponseObject> resp) {
          CHECK_OK(resp);
          load_finished.Notify();
        }));
  }
  CHECK(load_finished.WaitForNotificationWithTimeout(kTimeout));

  auto request = InvocationStrRequest<>{
      .id = "foo",
      .version_string = "v1",
      .handler_name = "Handler",
  };

  for (auto _ : state) {
    absl::Notification execute_finished;
    CHECK_OK(
        roma_service->Execute(std::make_unique<InvocationStrRequest<>>(request),
                              [&](absl::StatusOr<ResponseObject> resp) {
                                CHECK_OK(resp);
                                execute_finished.Notify();
                              }));
    CHECK(execute_finished.WaitForNotificationWithTimeout(kTimeout));
  }
}

void BM_ObjectAsStructInput(::benchmark::State& state) {
  state.SetLabel(MakeLabel(absl::StrCat(state.name(), "_", state.range(0))));
  absl::Notification load_finished;
  {
    std::string js_code = absl::Substitute(R"""(
      let obj = $0;
      function Handler() {
        return struct_obj_callback(obj);
      }
        )""",
                                           GetObject(state.range(0)));
    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = js_code,
    });
    CHECK_OK(roma_service->LoadCodeObj(
        std::move(code_obj), [&](absl::StatusOr<ResponseObject> resp) {
          CHECK_OK(resp);
          load_finished.Notify();
        }));
  }
  CHECK(load_finished.WaitForNotificationWithTimeout(kTimeout));

  auto request = InvocationStrRequest<>{
      .id = "foo",
      .version_string = "v1",
      .handler_name = "Handler",
  };

  for (auto _ : state) {
    absl::Notification execute_finished;
    CHECK_OK(
        roma_service->Execute(std::make_unique<InvocationStrRequest<>>(request),
                              [&](absl::StatusOr<ResponseObject> resp) {
                                CHECK_OK(resp);
                                execute_finished.Notify();
                              }));
    CHECK(execute_finished.WaitForNotificationWithTimeout(kTimeout));
  }
}

BENCHMARK(BM_ObjectAsSerializedStringInput)
    ->Setup(DoSetup)
    ->Teardown(DoTeardown)
    ->RangeMultiplier(10)
    ->Range(10, 10'000);
BENCHMARK(BM_ObjectAsStructInput)
    ->Setup(DoSetup)
    ->Teardown(DoTeardown)
    ->RangeMultiplier(10)
    ->Range(10, 10'000);

}  // namespace

BENCHMARK_MAIN();
