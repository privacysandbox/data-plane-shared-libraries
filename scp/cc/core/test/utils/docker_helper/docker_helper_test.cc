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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/container/btree_map.h"

using ::testing::StrEq;

namespace google::scp::core::test {
TEST(DockerHelper, PortMapToSelf) {
  EXPECT_THAT(PortMapToSelf("8080"), StrEq("8080:8080"));
}

TEST(DockerHelper, BuildStartContainerCmd) {
  EXPECT_THAT(
      BuildStartContainerCmd("", "container_name", "image_name", "9000:8000"),
      StrEq("docker -D run --rm -itd --privileged "
            "--name=container_name -p "
            "9000:8000 image_name"));

  EXPECT_THAT(BuildStartContainerCmd("network", "container_name", "image_name",
                                     "9000:8000"),
              StrEq("docker -D run --rm -itd --privileged --network=network "
                    "--name=container_name -p "
                    "9000:8000 image_name"));

  absl::btree_map<std::string, std::string> envs({
      {"host_address", "0.0.0.0"},
      {"host_port", "8080"},
  });
  EXPECT_THAT(BuildStartContainerCmd("network", "container_name", "image_name",
                                     "9000:9000", "1234-1240:1234", envs),
              StrEq("docker -D run --rm -itd --privileged --network=network "
                    "--name=container_name "
                    "-p 9000:9000 -p 1234-1240:1234 "
                    "--env host_address=0.0.0.0 --env host_port=8080 "
                    "image_name"));

  EXPECT_THAT(BuildStartContainerCmd("network", "container_name", "image_name",
                                     "9000:9000", "1234-1240:1234", envs,
                                     "-v /tmp:/tmp"),
              StrEq("docker -D run --rm -itd --privileged --network=network "
                    "--name=container_name "
                    "-p 9000:9000 -p 1234-1240:1234 "
                    "--env host_address=0.0.0.0 --env host_port=8080 "
                    "-v /tmp:/tmp "
                    "image_name"));
}

TEST(DockerHelper, BuildCreateImageCmd) {
  EXPECT_THAT(
      BuildCreateImageCmd("image_target"),
      StrEq(
          "bazel build --action_env=BAZEL_CXXOPTS='-std=c++17' image_target"));
  EXPECT_THAT(
      BuildCreateImageCmd("image_target", "--p1=p1 --p2=p2 --p3=p3"),
      StrEq("bazel build --action_env=BAZEL_CXXOPTS='-std=c++17' image_target "
            "--p1=p1 --p2=p2 --p3=p3"));
}

TEST(DockerHelper, BuildLoadImageCmd) {
  EXPECT_THAT(BuildLoadImageCmd("image_name"),
              StrEq("docker load < image_name"));
}

TEST(DockerHelper, BuildCreateNetworkCmd) {
  EXPECT_THAT(BuildCreateNetworkCmd("network_name"),
              StrEq("docker network create network_name"));
}

TEST(DockerHelper, BuildRemoveNetworkCmd) {
  EXPECT_THAT(BuildRemoveNetworkCmd("network_name"),
              StrEq("docker network rm network_name"));
}

TEST(DockerHelper, BuildStopContainerCmd) {
  EXPECT_THAT(BuildStopContainerCmd("container_name"),
              StrEq("docker rm -f container_name"));
}
}  // namespace google::scp::core::test
