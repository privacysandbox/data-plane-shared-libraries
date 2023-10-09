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

#pragma once

#ifdef DEBUG_GRPC_TAG_MANAGER
#include <chrono>

#include "core/common/time_provider/src/time_provider.h"
#endif
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace google::scp::core {
/**
 * @brief A manager class of GRPC tags.
 *
 * The GRPC tags are raw, unmanaged pointers. To avoid manual memory management,
 * this class uses an internal map to keep track of all the pointers, and
 * manages the lifetimes of them.
 *
 * @tparam T The type of the tag.
 * @tparam Allocator The allocator of \p T. This allows us later plug in arena
 * allocators.
 */
template <typename T, class Allocator = std::allocator<T>>
class GrpcTagManager {
 public:
  using Tag = std::remove_pointer_t<T>;

  /// Allocate a tag and register it.
  template <class... Args>
  T* Allocate(Args&&... args) {
    auto ptr = std::allocate_shared<T, Allocator>(allocator_,
                                                  std::forward<Args>(args)...);
    auto* ret = ptr.get();
    registry_.insert({ptr.get(), RegistryItem(std::move(ptr))});
    return ret;
  }

  /// De-register a tag.
  void Deallocate(void* tag) { registry_.erase(tag); }

  /// Size of the registry
  size_t Size() { return registry_.size(); }

 private:
#ifdef DEBUG_GRPC_TAG_MANAGER
  // This may be useful when we suspect memleaks from unattended tags.
  struct RegistryItem {
    explicit RegistryItem(std::shared_ptr<Tag>&& ptr)
        : ptr(ptr),
          timestamp(common::TimeProvider::
                        GetSteadyTimestampInNanosecondsAsClockTicks()) {}

    std::shared_ptr<Tag> ptr;
    uint64_t timestamp;
  };
#else
  using RegistryItem = std::shared_ptr<Tag>;
#endif  // DEBUG_GRPC_TAG_MANAGER
  struct Hash {
    size_t operator()(void* const p) const {
      return reinterpret_cast<size_t>(p);
    }
  };

  Allocator allocator_;
  std::unordered_map<void*, RegistryItem, Hash> registry_;
};

}  // namespace google::scp::core
