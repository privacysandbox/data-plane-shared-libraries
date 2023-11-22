## Overview

The `run_within_container.sh` script allows for building a given bazel target within a container
with fixed dependencies.

The container is downloaded from the AWS ECR repository:

`public.ecr.aws/t3i9g2s2/cc-build-linux-x86-64`

**See the last section of this readme for information on how to push a new version.**

Triggering a build pre-requisites:

1. This script must be run within this git repository
2. The path to the bazel target has to be absolute from the top of the repository
3. The output directory specified to put the build artifacts must be writable by the current user
4. Docker must be installed and available in the host machine
5. Docker must be executable sudoless

### Example usage

```sh
scp/cc/tools/build/run_within_container.sh --bazel_command="bazel build //scp/cc/pbs/budget_key/src:pbs_budget_key_lib" --bazel_output_directory=/tmp/my_output_dir
```

In the command above:

`--bazel_command` is the command to be executed. In this case it is `bazel build`.

Note that the path to the target is absolute form the top of the repository. This is required.

`--bazel_output_directory` is a directory in the host machine running this script, where the build
artifacts will be placed.

**This directory should ideally be under `/tmp`**

The build will create a directory under the given `bazel_output_directory` prefixed with
`scp_cc_build_`

If the output directory is not important, this can be set to `$(mktemp -d)` to just use a generic
tmp directory. e.g. `--bazel_output_directory=$(mktemp -d)`

### Another example

To run all the tests (except targets under scp/cc/tools/build/) within the container:

```sh
scp/cc/tools/build/run_within_container.sh --bazel_command="bazel test //..." --bazel_output_directory=/tmp/my_output_dir
```

#

## Pushing a new version of the container

### Kokoro container

**NOTE: Must have setup the AWS CLI, and the AWS account that is configured must have access to
resources in account 221820322062.**

This is only needed when new dependencies need to be added to the container.

The container image used to build is generated with the bazel rule `container_to_build_cc` within
this same directory. And then this image is uploaded to AWS ECR repository:
`public.ecr.aws/t3i9g2s2/cc-build-linux-x86-64`

To push a new version:

```sh
$ bazel build //scp/cc/tools/build:container_to_build_cc

# At top of repo
$ docker load < bazel-bin/scp/cc/tools/build/container_to_build_cc_commit.tar

# Update new version tag in cc/tools/build/build_container_params.bzl

# Replace <NEW_VERSION> with new version tag.
$ docker tag bazel/scp/cc/tools/build:container_to_build_cc public.ecr.aws/t3i9g2s2/cc-build-linux-x86-64:<NEW_VERSION>

$ aws ecr-public get-login-password \
  --region us-east-1 | docker login \
  --username AWS \
  --password-stdin public.ecr.aws

# Replace <NEW_VERSION> with new version tag.
$ docker push public.ecr.aws/t3i9g2s2/cc-build-linux-x86-64:<NEW_VERSION>
```
