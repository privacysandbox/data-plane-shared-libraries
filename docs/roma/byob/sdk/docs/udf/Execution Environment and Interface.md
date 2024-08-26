# Execution environment and interface

## Execution environment and libraries

The UDF will execute in a container with the base image being a
[Google distroless container image](https://github.com/GoogleContainerTools/distroless). The
distroless images currently supported are:

<!-- prettier-ignore-start -->
<!-- markdownlint-disable line-length -->
| ARCHITECTURE | IMAGE             | TAG           | HASH (SHA256)                                                      |
| ------------ | ------------------------------ | ------------- | ------------------------------------------------------------------ |
| AMD64        | gcr.io/distroless/cc-debian11 | nonroot-amd64 | `5a9e854bab8498a61a66b2cfa4e76e009111d09cb23a353aaa8d926e29a653d9` |
| ARM64        | gcr.io/distroless/cc-debian11 | nonroot-arm64 | `3122cd55375a0a9f32e56a18ccd07572aeed5682421432701a03c335ab79c650` |
<!-- markdownlint-enable line-length -->
<!-- prettier-ignore-end -->

These images contain a minimal Linux, glibc runtime for "mostly-statically compiled" languages like
C/C++, GO, Rust, D, among others.

Images can be downloaded using the IMAGE and HASH values from the table above. To retrieve the image
using docker:

```shell
docker pull IMAGE@sha256:HASH
```

UDFs must be provided as a single, self-contained executable file. The executable may depend on
shared libraries contained within the base distroless image, for example, `glibc`, `libgcc1` and its
dependencies.

> An easy way of ensuring all the required dependencies are available is to statically compile the
> binary.

## Command-line flags

Command line flag are used to pass the file descriptor (fd) over which the UDF can communicate. The
first positional argument to the UDF is the fd for communication.

For details about how to use the passed flag(s), see
[communication interface](/docs/roma/byob/sdk/docs/udf/Communication%20Interface.md) doc and
[examples](/src/roma/byob/udf/).

## gVisor

gVisor supports a large subset of Linux syscalls; some syscalls may have a partial implementation.
Refer to list of syscalls and gVisor's support status for
[amd64](/docs/roma/byob/sdk/docs/udf/amd64-syscalls.md) and
[arm64](/docs/roma/byob/sdk/docs/udf/arm64-syscalls.md) CPU architectures.
