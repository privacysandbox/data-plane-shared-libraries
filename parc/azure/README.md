# Parc Azure

This repository contains a reference implementation for the Parc server on Azure.

## Building the Parc Azure server

The Parc Azure server is built using [Bazel](https://bazel.build/docs).

The file `Dockerfile` in this directory is used to build the Parc Azure server. The image can be
built using the following command executed in the current directory:

```sh
docker buildx build \
  --tag=privacysandbox/parc/parc-azure-server:latest \
  --build-context repo-root=../.. \
  .
```

Note the `--build-context` flag pointing to the repository root.
