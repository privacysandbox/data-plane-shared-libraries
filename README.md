# Privacy Sandbox Common

Common repository for shared across Privacy Sandbox server teams.

### Telemetry

Importing the telemetry in this package requires dependencies in `cpp_deps.bzl` and the
opentelemetry dependencies. Example import:

```bazel
http_archive(
    name = "privacy_sandbox_shared",
    sha256 = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
    strip_prefix = "data-plane-shared-libraries-xxxxxx.0",
    urls = [
        "https://github.com/privacysandbox/data-plane-shared-libraries/archive/refs/tags/xxxxxx.tar.gz",
    ],
)

load("@privacy_sandbox_shared//:cpp_deps.bzl", "cpp_dependencies")

cpp_dependencies()

load("@io_opentelemetry_cpp//bazel:repository.bzl", "opentelemetry_cpp_deps")

opentelemetry_cpp_deps()
```

Additionally the `with_abseil` flag should be set for OpenTelemetry. In your `.bazelrc` include:

```bazel
build --@io_opentelemetry_cpp//api:with_abseil
```
