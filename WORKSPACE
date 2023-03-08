workspace(name = "google_privacysandbox_servers_common")

load("//:deps.bzl", repo_dependencies = "dependencies")

repo_dependencies()

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")

bazel_skylib_workspace()

load("@rules_jvm_external//:repositories.bzl", "rules_jvm_external_deps")

rules_jvm_external_deps()

load("@rules_jvm_external//:setup.bzl", "rules_jvm_external_setup")

rules_jvm_external_setup()

load("@google_privacysandbox_servers_common//third_party:maven_deps.bzl", "maven_deps")

maven_deps()

load(
    "@io_bazel_rules_docker//repositories:repositories.bzl",
    container_repositories = "repositories",
)

container_repositories()

load("@io_bazel_rules_docker//repositories:deps.bzl", docker_container_deps = "deps")

docker_container_deps()

load("//third_party:container_deps.bzl", "container_deps")

container_deps()

load(
    "@io_bazel_rules_docker//java:image.bzl",
    java_image_repos = "repositories",
)

java_image_repos()
