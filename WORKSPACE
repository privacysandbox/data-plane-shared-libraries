workspace(name = "google_privacysandbox_servers_common")

load("//builders/bazel:deps.bzl", "python_deps", "python_register_toolchains")

python_deps()

python_register_toolchains("//builders/bazel")

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

load("@rules_jvm_external//:repositories.bzl", "rules_jvm_external_deps")

rules_jvm_external_deps()

load("@rules_jvm_external//:setup.bzl", "rules_jvm_external_setup")

rules_jvm_external_setup()

load("@google_privacysandbox_servers_common//third_party:container_deps.bzl", "container_deps")

container_deps()

local_repository(
    name = "google_privacysandbox_functionaltest_system",
    path = "testing/functionaltest-system",
)
