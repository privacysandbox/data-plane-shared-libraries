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

#ifndef ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_CPU_PROFILER_ISOLATE_WRAPPER_H_
#define ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_CPU_PROFILER_ISOLATE_WRAPPER_H_

#include <memory>
#include <string>
#include <utility>

#include "include/v8-profiler.h"
#include "include/v8.h"
#include "src/core/common/uuid/uuid.h"
#include "src/roma/sandbox/js_engine/v8_engine/v8_isolate_wrapper.h"

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {

class CpuProfilerIsolateWrapperImpl final : public V8IsolateWrapper {
 public:
  CpuProfilerIsolateWrapperImpl(
      absl::Nonnull<v8::Isolate*> isolate,
      std::unique_ptr<v8::ArrayBuffer::Allocator> allocator)
      : V8IsolateWrapper(),
        isolate_(isolate),
        allocator_(std::move(allocator)),
        cpu_profiler_(v8::CpuProfiler::New(isolate_)) {
    StartProfiling();
  }

  ~CpuProfilerIsolateWrapperImpl() override {
    StopProfiling();
    // Isolates are only deleted this way and not with Free().
    isolate_->Dispose();
  }

  // Not copyable or moveable.
  CpuProfilerIsolateWrapperImpl(const CpuProfilerIsolateWrapperImpl&) = delete;
  CpuProfilerIsolateWrapperImpl& operator=(
      const CpuProfilerIsolateWrapperImpl&) = delete;

  v8::Isolate* isolate() override { return isolate_; }

 private:
  void StartProfiling() {
    v8::HandleScope handle_scope(isolate_);
    auto isolate_uuid = google::scp::core::common::Uuid::GenerateUuid();
    std::string uuid_str = google::scp::core::common::ToString(isolate_uuid);
    cpu_profile_name_.Reset(
        isolate_,
        v8::String::NewFromUtf8(isolate_, uuid_str.c_str()).ToLocalChecked());
    cpu_profiler_->SetSamplingInterval(1);
    bool record_samples = true;
    cpu_profiler_->StartProfiling(cpu_profile_name_.Get(isolate_),
                                  record_samples);
  }

  void StopProfiling() {
    v8::HandleScope handle_scope(isolate_);
    auto profile =
        cpu_profiler_->StopProfiling(cpu_profile_name_.Get(isolate_));
    std::cout << "Number of Samples: " << profile->GetSamplesCount()
              << std::endl;
    std::cout << "Total Execution Time: "
              << profile->GetEndTime() - profile->GetStartTime() << " us"
              << std::endl;

    if (profile->GetSamplesCount() > 1) {
      int totalInterval = 0;
      for (int i = 1; i < profile->GetSamplesCount(); i++) {
        int currentTimestamp = profile->GetSampleTimestamp(i);
        int previousTimestamp = profile->GetSampleTimestamp(i - 1);
        totalInterval += currentTimestamp - previousTimestamp;
      }

      double averageInterval = totalInterval / (profile->GetSamplesCount() - 1);
      std::cout << "Average Sampling Interval: " << averageInterval << " us"
                << std::endl;
    } else {
      std::cout << "Not enough samples to calculate interval." << std::endl;
    }

    auto hitCounts = GetFunctionCounts(profile);
    AnalyzeProfileNode(hitCounts, profile->GetTopDownRoot());
    cpu_profiler_->Dispose();
  }

  void AnalyzeProfileNode(
      absl::flat_hash_map<std::string, int>& function_counts,
      const v8::CpuProfileNode* node, int depth = 0) {
    // Indentation for visual clarity
    for (int i = 0; i < depth; ++i) std::cout << "  ";

    std::string functionName =
        *v8::String::Utf8Value(isolate_, node->GetFunctionName());
    int hitCount = function_counts[functionName];
    std::cout << functionName << " (Hit count: " << hitCount << ")\n";

    // Recursively analyze child nodes
    for (int i = 0; i < node->GetChildrenCount(); ++i) {
      AnalyzeProfileNode(function_counts, node->GetChild(i), depth + 1);
    }
  }

  absl::flat_hash_map<std::string, int> GetFunctionCounts(
      v8::CpuProfile* profile) {
    absl::flat_hash_map<std::string, int> hitCounts;

    for (int i = 0; i < profile->GetSamplesCount(); i++) {
      const v8::CpuProfileNode* node = profile->GetSample(i);

      // Increment counts for every function in the current sample's callstack
      while (node != nullptr) {
        v8::Local<v8::String> functionName = node->GetFunctionName();
        std::string functionNameStr =
            *v8::String::Utf8Value(isolate_, functionName);
        hitCounts[functionNameStr]++;
        node = node->GetParent();
      }
    }
    return hitCounts;
  }

  v8::Isolate* isolate_;
  // Each isolate has an allocator that lives with it:
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator_;
  v8::Persistent<v8::String> cpu_profile_name_;
  v8::CpuProfiler* cpu_profiler_;
};

class V8IsolateFactory {
 public:
  /**
   * @brief Factory to create V8IsolateWrapper.
   *
   * @return std::unique_ptr<V8IsolateWrapper> created V8IsolateWrapper.
   */
  static absl::Nonnull<std::unique_ptr<V8IsolateWrapper>> Create(
      absl::Nonnull<v8::Isolate*> isolate,
      absl::Nonnull<std::unique_ptr<v8::ArrayBuffer::Allocator>> allocator,
      bool enable_cpu_profiler) {
    if (enable_cpu_profiler) {
      return std::make_unique<CpuProfilerIsolateWrapperImpl>(
          isolate, std::move(allocator));
    }
    return std::make_unique<V8IsolateWrapperImpl>(isolate,
                                                  std::move(allocator));
  }
};
}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine

#endif  // ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_CPU_PROFILER_ISOLATE_WRAPPER_H_
