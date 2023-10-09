## Start running container

The `start_container_local.sh` script will start running a container with fixed dependencies.

The container is downloaded from the AWS ECR repository:
`221820322062.dkr.ecr.us-east-1.amazonaws.com/admcloud-scp`

Triggering a build pre-requisites:

1. This script must be run within this git repository
2. The aws cli needs to be installed
3. The aws account configured must have access to resources in account 221820322062
4. Docker must be installed and available in the host machine
5. Docker must be executable sudoless

### Example usage

```sh
cc/tools/build/local/start_container_local.sh
```

## Build within container

The `bazel_build_within_container.sh` script allows for building a given bazel target within a
container.

The bazel output files are always under `/tmp/bazel_build_output`.

Please exclude the build targets under `//scp/cc/tools/build` as the build time for container takes
more than an hour.

### Example usage

To build and run all the tests within the container:

```sh
cc/tools/build/local/bazel_build_within_container.sh --bazel_command="bazel build //cc/... && bazel test //cc/..."
```

## Stop the running container

The `stop_container_local.sh` script will stop the running container.

### Example usage

```sh
cc/tools/build/local/stop_container_local.sh
```

## Pushing a new version of the container

Please read `README.md` under cc\tools\build.
