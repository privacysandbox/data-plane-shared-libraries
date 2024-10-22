# Guide to the Roma V8 UDF SDK

## Background

Roma V8 is an open-source C++ library that provides a sandboxed environment for executing untrusted
user-defined functions (UDFs) written in JavaScript and/or WASM.

Roma V8 does this by using two layers of sandboxing:

1. [V8](https://v8.dev/docs): V8 is Google's open source high-performance JavaScript and WebAssembly
   engine, written in C++. It is used in Chromium, the open-source browser project that forms the
   basis for Google Chrome and other browsers, as well as in Node.js, among others.

    Untrusted code runs in its own sandboxed environment context in one isolate within V8. An
    isolate is a VM instance of the V8 engine with its own memory space. A context is an execution
    environment that allows separate, unrelated, JavaScript code to run in a single instance of V8.
    [V8's security model](https://v8.dev/docs/embed#security-model) is based on the "same-origin
    policy," which prevents code from one website from accessing or modifying the data of another
    website. In V8, an "origin" is defined as a context.

1. [Sandboxed API](https://developers.google.com/code-sandboxing/sandboxed-api): SAPI is a wrapper
   of [Google Sandbox2](https://developers.google.com/code-sandboxing/sandbox2), which is an
   open-source C++ security sandbox for Linux. In Roma, sandbox2 is used to sandbox the V8 engine to
   provide the second layer of security. Roma sets up
   [Sandbox Policy](https://developers.google.com/code-sandboxing/sandbox2/explained#sandbox_policy),
   which uses seccomp-bpf, a way to restrict system calls, to only allow the system calls that V8
   needs to run. For example, the sandboxee cannot access networking system calls.

## CLI Tools

The Roma V8 UDF SDK comes packaged with CLI tools to debug and benchmark UDFs.

-   [Roma Shell](tools/shell_cli.md): An interactive REPL interface for loading and executing User
    Defined Functions (UDFs)
-   [Roma Benchmark](tools/udf_benchmark_cli.md): An interface built on top of Google's
    microbenchmarking library for loading and executing a given UDF. Each load/execute operation is
    benchmarked independently.

These CLI tools are packaged as docker images in the tools/ subdirectory of the SDK. To run said
images in the SDK directory, first load the respective docker image:

```[sh]
docker load -i tools/{SDK_NAME}_{TOOL_NAME}_image.tar
```

This command, if successful, should output `Loaded image: {DOCKER_IMAGE_TAG}`

To run this loaded image, run:

```[sh]
docker run -i {DOCKER_IMAGE_TAG}
```

### Example

Given a `roma_v8_sdk` target named `test_service_sdk`, to run the Roma Shell CLI tool, run:

```[sh]
docker load -i test_service_sdk/tools/roma-v8-shell.tar
docker run -i privacy_sandbox/roma-v8/roma-shell:v1
```
