## Tink Defs

This folder contains the necessary Bazel rules for using Tink directly from GitHub as well as
maintaining local patches for changes not yet upstreamed.

This is also useful for testing changes in Tink not yet published to Maven.

This should be used for development only and not official releases.

## Patches

A patch against Tink can be generated with `git diff > out.patch` from within a checked out copy of
Tink but any filenames must be rewritten to remove `java_src` because `java_src` is the root of the
imported Bazel repository.

Patches can be added to this folder and to the "patches" list in tink_defs.bzl
