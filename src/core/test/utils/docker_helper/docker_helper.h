// Copyright 2022 Google LLC
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

#ifndef CORE_TEST_UTILS_DOCKER_HELPER_DOCKER_HELPER_H_
#define CORE_TEST_UTILS_DOCKER_HELPER_DOCKER_HELPER_H_

#include <string>
#include <string_view>

#include "absl/container/btree_map.h"

namespace google::scp::core::test {
std::string PortMapToSelf(std::string_view port);

int StartLocalStackContainer(std::string_view network,
                             std::string_view container_name,
                             std::string_view exposed_port);

int StartGcpContainer(std::string_view network, std::string_view container_name,
                      std::string_view exposed_port);

int StartContainer(
    std::string_view network, std::string_view container_name,
    std::string_view image_name, std::string_view port_mapping1,
    std::string_view port_mapping2 = "",
    const absl::btree_map<std::string, std::string>& environment_variables = {},
    std::string_view addition_args = "");

int CreateImage(std::string_view image_target, std::string_view args = "");

int LoadImage(std::string_view image_name);

int CreateNetwork(std::string_view network_name);

int RemoveNetwork(std::string_view network_name);

int StopContainer(std::string_view container_name);

std::string BuildStopContainerCmd(std::string_view container_name);

std::string BuildRemoveNetworkCmd(std::string_view network_name);

std::string BuildCreateNetworkCmd(std::string_view network_name);

std::string BuildLoadImageCmd(std::string_view image_name);

std::string BuildCreateImageCmd(std::string_view image_target,
                                std::string_view args = "");

std::string BuildStartContainerCmd(
    std::string_view network, std::string_view container_name,
    std::string_view image_name, std::string_view port_mapping1,
    std::string_view port_mapping2 = "",
    const absl::btree_map<std::string, std::string>& environment_variables = {},
    std::string_view addition_args = "");

/**
 * @brief Get the Ip Address of a docker container.
 *
 * @param network_name the network the container is in.
 * @param container_name the container name.
 * @return std::string the returned IP address.
 */
std::string GetIpAddress(std::string_view network_name,
                         std::string_view container_name);

/**
 * @brief Run docker command to grant 666 permission to the given folder inside
 * the given container.
 *
 * @param container_name the name of the given container.
 * @param folder the given folder.
 */
void GrantPermissionToFolder(std::string_view container_name,
                             std::string_view folder);
}  // namespace google::scp::core::test

#endif  // CORE_TEST_UTILS_DOCKER_HELPER_DOCKER_HELPER_H_
