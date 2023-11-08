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

#include "docker_helper.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/substitute.h"

// localstack version is pinned so that tests are repeatable
static constexpr char kLocalstackImage[] = "localstack/localstack:1.0.3";
// gcloud SDK tool version is pinned so that tests are repeatable
static constexpr char kGcpImage[] =
    "gcr.io/google.com/cloudsdktool/google-cloud-cli:380.0.0-emulators";

namespace google::scp::core::test {
std::string PortMapToSelf(std::string_view port) {
  return absl::StrCat(port, ":", port);
}

int StartLocalStackContainer(const std::string& network,
                             const std::string& container_name,
                             const std::string& exposed_port) {
  const absl::btree_map<std::string, std::string> env_variables{
      {"EDGE_PORT", exposed_port},
  };
  return StartContainer(network, container_name, kLocalstackImage,
                        PortMapToSelf(exposed_port), "4510-4559",
                        env_variables);
}

int StartGcpContainer(const std::string& network,
                      const std::string& container_name,
                      const std::string& exposed_port) {
  absl::btree_map<std::string, std::string> env_variables;
  return StartContainer(network, container_name, kGcpImage,
                        PortMapToSelf(exposed_port), "9000-9050",
                        env_variables);
}

int StartContainer(
    const std::string& network, const std::string& container_name,
    const std::string& image_name, const std::string& port_mapping1,
    const std::string& port_mapping2,
    const absl::btree_map<std::string, std::string>& environment_variables,
    const std::string& addition_args) {
  return std::system(BuildStartContainerCmd(
                         network, container_name, image_name, port_mapping1,
                         port_mapping2, environment_variables, addition_args)
                         .c_str());
}

std::string BuildStartContainerCmd(
    const std::string& network, const std::string& container_name,
    const std::string& image_name, const std::string& port_mapping1,
    const std::string& port_mapping2,
    const absl::btree_map<std::string, std::string>& environment_variables,
    const std::string& addition_args) {
  std::string ports_mapping = absl::Substitute("-p $0 ", port_mapping1);
  if (!port_mapping2.empty()) {
    ports_mapping += absl::Substitute("-p $0 ", port_mapping2);
  }

  std::string name_network;
  if (!network.empty()) {
    name_network = absl::Substitute("--network=$0 ", network);
  }

  std::string envs;
  for (const auto& [var_name, value] : environment_variables) {
    envs += absl::Substitute("--env $0=$1 ", var_name, value);
  }

  return absl::Substitute(
      "docker -D run --rm -itd --privileged "
      "$0"
      "--name=$1 "
      "$2"
      "$3"
      "$4"
      "$5",
      name_network, container_name, ports_mapping, envs,
      addition_args.empty() ? addition_args : addition_args + " ", image_name);
}

int CreateImage(const std::string& image_target, const std::string& args) {
  return std::system(BuildCreateImageCmd(image_target, args).c_str());
}

std::string BuildCreateImageCmd(const std::string& image_target,
                                const std::string& args) {
  std::string cmd =
      "bazel build --action_env=BAZEL_CXXOPTS='-std=c++17' " + image_target;
  if (!args.empty()) {
    absl::StrAppend(&cmd, " ", args);
  }
  return cmd;
}

int LoadImage(const std::string& image_name) {
  return std::system(BuildLoadImageCmd(image_name).c_str());
}

std::string BuildLoadImageCmd(const std::string& image_name) {
  return "docker load < " + image_name;
}

int CreateNetwork(const std::string& network_name) {
  return std::system(BuildCreateNetworkCmd(network_name).c_str());
}

std::string BuildCreateNetworkCmd(const std::string& network_name) {
  return "docker network create " + network_name;
}

int RemoveNetwork(const std::string& network_name) {
  return std::system(BuildRemoveNetworkCmd(network_name).c_str());
}

std::string BuildRemoveNetworkCmd(const std::string& network_name) {
  return "docker network rm " + network_name;
}

int StopContainer(const std::string& container_name) {
  return std::system(BuildStopContainerCmd(container_name).c_str());
}

std::string BuildStopContainerCmd(const std::string& container_name) {
  return "docker rm -f " + container_name;
}

std::string GetIpAddress(const std::string& network_name,
                         const std::string& container_name) {
  char buffer[20];
  std::string result;
  std::string command =
      absl::StrCat("docker inspect -f '{{ .NetworkSettings.Networks.",
                   network_name, ".IPAddress }}' ", container_name);
  auto pipe = popen(command.c_str(), "r");
  if (!pipe) {
    throw std::runtime_error("Failed to fetch IP address for container!");
    return "";
  }

  while (fgets(buffer, 20, pipe) != nullptr) {
    result += buffer;
  }

  auto return_status = pclose(pipe);
  if (return_status == EXIT_FAILURE) {
    throw std::runtime_error(
        "Failed to close pipe to fetch IP address for container!");
    return "";
  }

  auto length = result.length();
  if (result.back() == '\n') {
    length -= 1;
  }
  return result.substr(0, length);
}

void GrantPermissionToFolder(const std::string& container_name,
                             const std::string& folder) {
  std::string s =
      absl::StrCat("docker exec -itd ", container_name, " chmod 666 ", folder);
  auto result = std::system(s.c_str());
  if (result != 0) {
    throw std::runtime_error("Failed to grant permission!");
  } else {
    std::cout << "Succeeded to grant permission!" << std::endl;
  }
}
}  // namespace google::scp::core::test
