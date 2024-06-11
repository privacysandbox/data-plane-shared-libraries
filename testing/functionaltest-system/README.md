# Privacy Sandbox Functional Testing Tools

This repository contains [Bazel Starlark Extentions](https://bazel.build/extending/concepts) and
tools used by the [Privacy Sandbox](https://github.com/privacysandbox) open-source ecosystem for
functional testing.

The bazel extensions provide support for testing RPC endpoints, using `bazel test` as the test
runner.

## Features at a glance

-   Testing RPC endpoints using either gRPC or HTTP
-   Declarative, "code-free" approach to integration, functional or load testing
-   diff testing of RPC endpoints
-   load testing of RPC endpoints

## Getting started

### Prerequisites

The `functionaltest-system` has been tested using Bazel 5.x. Check your Bazel version by running
`bazel --version`.

### Testing the functional test suite

To run the tests in this repo, you will need to a few tools available in your environment:

-   [golang](https://go.dev/) compiler &mdash we tested with golang v1.19
-   C++ compiler &mdash; we tested with clang v14
-   [Docker Compose v2](https://github.com/docker/compose#where-to-get-docker-compose)

Note: These dependencies are used specifically to build or run the example gRPC servers for use as
test subjects. If you do not run tests of this repo itself, there is no need to install any of these
tools.

## Examples

Usage of this repo is demonstrated in a workspace in the `examples/grpc_greeter` directory.
