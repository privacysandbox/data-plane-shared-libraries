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

#include <chrono>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

using std::runtime_error;

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
  const absl::flat_hash_map<std::string, std::string> env_variables{
      {"EDGE_PORT", exposed_port},
  };
  return StartContainer(network, container_name, kLocalstackImage,
                        PortMapToSelf(exposed_port), "4510-4559",
                        env_variables);
}

int StartGcpContainer(const std::string& network,
                      const std::string& container_name,
                      const std::string& exposed_port) {
  absl::flat_hash_map<std::string, std::string> env_variables;
  return StartContainer(network, container_name, kGcpImage,
                        PortMapToSelf(exposed_port), "9000-9050",
                        env_variables);
}

int StartContainer(
    const std::string& network, const std::string& container_name,
    const std::string& image_name, const std::string& port_mapping1,
    const std::string& port_mapping2,
    const absl::flat_hash_map<std::string, std::string>& environment_variables,
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
    const absl::flat_hash_map<std::string, std::string>& environment_variables,
    const std::string& addition_args) {
  auto ports_mapping = absl::StrFormat("-p %s ", port_mapping1);
  if (!port_mapping2.empty()) {
    ports_mapping += absl::StrFormat("-p %s ", port_mapping2);
  }

  std::string name_network;
  if (!network.empty()) {
    name_network = absl::StrFormat("--network=%s ", network);
  }

  std::string envs;
  for (auto it = environment_variables.begin();
       it != environment_variables.end(); ++it) {
    envs += absl::StrFormat("--env %s=%s ", it->first, it->second);
  }

  return absl::StrFormat(
      "docker -D run --rm -itd --privileged "
      "%s"
      "--name=%s "
      "%s"
      "%s"
      "%s"
      "%s",
      name_network, container_name, ports_mapping, envs,
      addition_args.empty() ? addition_args : addition_args + " ", image_name);
}

int CreateImage(const std::string& image_target, const std::string& args) {
  return std::system(BuildCreateImageCmd(image_target, args).c_str());
}

std::string BuildCreateImageCmd(const std::string& image_target,
                                const std::string& args) {
  auto cmd = absl::StrFormat(
      "bazel build --action_env=BAZEL_CXXOPTS='-std=c++17' %s", image_target);
  if (!args.empty()) {
    cmd += " " + args;
  }
  return cmd;
}

int LoadImage(const std::string& image_name) {
  return std::system(BuildLoadImageCmd(image_name).c_str());
}

std::string BuildLoadImageCmd(const std::string& image_name) {
  return absl::StrFormat("docker load < %s", image_name);
}

int CreateNetwork(const std::string& network_name) {
  return std::system(BuildCreateNetworkCmd(network_name).c_str());
}

std::string BuildCreateNetworkCmd(const std::string& network_name) {
  return absl::StrFormat("docker network create %s", network_name);
}

int RemoveNetwork(const std::string& network_name) {
  return std::system(BuildRemoveNetworkCmd(network_name).c_str());
}

std::string BuildRemoveNetworkCmd(const std::string& network_name) {
  return absl::StrFormat("docker network rm %s", network_name);
}

int StopContainer(const std::string& container_name) {
  return std::system(BuildStopContainerCmd(container_name).c_str());
}

std::string BuildStopContainerCmd(const std::string& container_name) {
  return absl::StrFormat("docker rm -f %s", container_name);
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
    throw runtime_error("Failed to fetch IP address for container!");
    return "";
  }

  while (fgets(buffer, 20, pipe) != nullptr) {
    result += buffer;
  }

  auto return_status = pclose(pipe);
  if (return_status == EXIT_FAILURE) {
    throw runtime_error(
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
    throw runtime_error("Failed to grant permission!");
  } else {
    std::cout << "Succeeded to grant permission!" << std::endl;
  }
}
}  // namespace google::scp::core::test
