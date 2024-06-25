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
#include "src/roma/sandbox/js_engine/v8_engine/heap_snapshot_parser.h"
#include "src/roma/sandbox/js_engine/v8_engine/v8_isolate_wrapper.h"

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {

class ProfilerIsolateWrapperImpl final : public V8IsolateWrapper {
 public:
  ProfilerIsolateWrapperImpl(
      absl::Nonnull<v8::Isolate*> isolate,
      std::unique_ptr<v8::ArrayBuffer::Allocator> allocator)
      : V8IsolateWrapper(),
        isolate_(isolate),
        allocator_(std::move(allocator)),
        cpu_profiler_(v8::CpuProfiler::New(isolate_)) {
    StartProfiling();
  }

  std::string StopProfiling() {
    v8::HandleScope handle_scope(isolate_);
    auto profile =
        cpu_profiler_->StopProfiling(cpu_profile_name_.Get(isolate_));

    std::string profiler_output = "Heap and CPU Profiler Output\n\n";
    absl::StrAppend(&profiler_output,
                    "Number of Samples: ", profile->GetSamplesCount(), "\n");
    absl::StrAppend(&profiler_output, "Total Execution Time: ",
                    profile->GetEndTime() - profile->GetStartTime(), " us\n");

    if (profile->GetSamplesCount() > 1) {
      int totalInterval = 0;
      for (int i = 1; i < profile->GetSamplesCount(); i++) {
        int currentTimestamp = profile->GetSampleTimestamp(i);
        int previousTimestamp = profile->GetSampleTimestamp(i - 1);
        totalInterval += currentTimestamp - previousTimestamp;
      }

      double averageInterval = totalInterval / (profile->GetSamplesCount() - 1);
      absl::StrAppend(&profiler_output,
                      "Average Sampling Interval: ", averageInterval, " us\n");
    } else {
      absl::StrAppend(
          &profiler_output,
          "Not enough samples to calculate average sampling interval.\n\n");
    }

    auto hitCounts = GetFunctionCounts(profile);
    absl::StrAppend(&profiler_output, "Stack Trace with Hit Counts: \n");
    std::string profile_nodes =
        AnalyzeProfileNode(hitCounts, profile->GetTopDownRoot());
    absl::StrAppend(&profiler_output, profile_nodes, "\n");
    cpu_profiler_->Dispose();

    absl::StrAppend(&profiler_output, "Heap Object Statistics:\n\n");

    for (int i = 0; i < isolate_->NumberOfTrackedHeapObjectTypes(); i++) {
      v8::HeapObjectStatistics stats;
      isolate_->GetHeapObjectStatisticsAtLastGC(&stats, i);
      absl::StrAppend(
          &profiler_output,
          absl::StrCat("Stats for Object Type: ", stats.object_type(), "\n"));
      absl::StrAppend(
          &profiler_output,
          absl::StrCat("  Object Subtype: ", stats.object_sub_type(), "\n"));
      absl::StrAppend(
          &profiler_output,
          absl::StrCat("  Object Count: ", stats.object_count(), "\n"));
      absl::StrAppend(
          &profiler_output,
          absl::StrCat("  Object Size: ", stats.object_size(), "\n\n"));
    }

    auto heap_profiler = isolate_->GetHeapProfiler();
    const v8::HeapSnapshot* snapshot = heap_profiler->TakeHeapSnapshot();

    std::string heap_output;
    HeapSnapshotParser parser(heap_output);
    snapshot->Serialize(&parser, v8::HeapSnapshot::SerializationFormat::kJSON);
    absl::StrAppend(&profiler_output, "Heap Snapshot: \n");
    absl::StrAppend(&profiler_output, heap_output, "\n");

    heap_profiler->StopTrackingHeapObjects();
    return profiler_output;
  }

  ~ProfilerIsolateWrapperImpl() override {
    // Isolates are only deleted this way and not with Free().
    isolate_->Dispose();
  }

  // Not copyable or moveable.
  ProfilerIsolateWrapperImpl(const ProfilerIsolateWrapperImpl&) = delete;
  ProfilerIsolateWrapperImpl& operator=(const ProfilerIsolateWrapperImpl&) =
      delete;

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
    bool track_allocations = true;
    isolate_->GetHeapProfiler()->StartTrackingHeapObjects(track_allocations);
  }

  std::string AnalyzeProfileNode(
      absl::flat_hash_map<std::string, int>& function_counts,
      const v8::CpuProfileNode* node, int depth = 0) {
    // Indentation for visual clarity
    std::string line(2 * depth, ' ');

    std::string functionName =
        *v8::String::Utf8Value(isolate_, node->GetFunctionName());
    int hitCount = function_counts[functionName];
    absl::StrAppend(&line, functionName, " (Hit count: ", hitCount, ")\n");

    // Recursively analyze child nodes
    for (int i = 0; i < node->GetChildrenCount(); ++i) {
      absl::StrAppend(&line, AnalyzeProfileNode(function_counts,
                                                node->GetChild(i), depth + 1));
    }
    return line;
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
      bool enable_profilers) {
    if (enable_profilers) {
      return std::make_unique<ProfilerIsolateWrapperImpl>(isolate,
                                                          std::move(allocator));
    }
    return std::make_unique<V8IsolateWrapperImpl>(isolate,
                                                  std::move(allocator));
  }
};
}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine

#endif  // ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_CPU_PROFILER_ISOLATE_WRAPPER_H_
