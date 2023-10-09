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

#include <memory>
#include <vector>

#include "cpio/server/interface/component_factory/component_factory_interface.h"

namespace google::scp::cpio {
/*! @copydoc ComponentFactoryInterface
 */
class ComponentFactory : public ComponentFactoryInterface {
 public:
  virtual ~ComponentFactory() = default;

  explicit ComponentFactory(std::vector<ComponentCreator> component_creators)
      : component_creators_(component_creators) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

 private:
  /// The list should be in dependency order. The component in the front should
  /// not depend on the component in the back.
  std::vector<ComponentCreator> component_creators_;
};
}  // namespace google::scp::cpio
