# Privacy Sandbox Common

Common repository for shared across Privacy Sandbox server teams.

### Bazel C++ Dependencies

To import this repo into your project, you first need to add it to your `WORKSPACE` file, using the
snippet provided in the release you choose:

```bazel
http_archive(
    name = "google_privacysandbox_servers_common",
    sha256 = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
    strip_prefix = "data-plane-shared-libraries-xxxxxx.0",
    urls = [
        "https://github.com/privacysandbox/data-plane-shared-libraries/archive/refs/tags/xxxxxx.tar.gz",
    ],
)

load("@google_privacysandbox_servers_common//third_party:cpp_deps.bzl", "cpp_dependencies")

cpp_dependencies()

load("@google_privacysandbox_servers_common//third_party:deps1.bzl", "deps1")

deps1()

load("@google_privacysandbox_servers_common//third_party:deps2.bzl", "deps2")

deps2()

load("@google_privacysandbox_servers_common//third_party:deps3.bzl", "deps3")

deps3()

load("@google_privacysandbox_servers_common//third_party:deps4.bzl", "deps4")

deps4()
```

Additionally the `with_abseil` flag should be set for OpenTelemetry. In your `.bazelrc` include:

```bazel
build --@io_opentelemetry_cpp//api:with_abseil
```

---

[![OpenSSF Scorecard](https://api.securityscorecards.dev/projects/github.com/privacysandbox/data-plane-shared-libraries/badge)](https://securityscorecards.dev/viewer/?uri=github.com/privacysandbox/data-plane-shared-libraries)
