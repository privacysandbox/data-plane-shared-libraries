# Changelog

All notable changes to this project will be documented in this file. See [commit-and-tag-version](https://github.com/absolute-version/commit-and-tag-version) for commit guidelines.

## 1.7.0 (2025-04-30)


### Features

* Add gojq as an alternative to jq


### Dependencies

* **deps:** Upgrade build-system to 1.0.1

## 1.6.0 (2025-04-25)


### Features

* Add container image tests


### Bug Fixes

* Update perf tests to set SUT_ID arg flag for jq


### Dependencies

* **deps:** Upgrade build-system to 1.0.0
* **deps:** Upgrade protobuf to 29.3

## 1.5.0 (2025-04-24)


### Features

* Add exclusion filter for sut files

## 1.4.0 (2025-04-24)


### Features

* Allow shell stage to propagate env vars


### Bug Fixes

* Add test for shell stage env vars
* Explicitly set permissions for all binaries
* Remove incorrect AZ env var

## 1.3.2 (2025-04-24)


### Bug Fixes

* Fix passing env vars to shell command
* Move azure-cli build to separate image stage
* Specify SUT_ID jq arg for perf tests

## 1.3.1 (2025-04-17)


### Bug Fixes

* Fix passing env vars to shell command

## 1.3.0 (2025-04-15)


### Features

* Add Azure CLI to functional test image
* Add key substitution to grpc diff test invoker
* Fix bazel test command with test_env
* Remove extra JQ substitution logic, pass SUT_ID to script instead
* Set SUT_ID for shell and bazel test commands

## 1.2.0 (2025-03-31)


### Features

* Add start and step as args to the promql_test macro

## 1.1.1 (2025-03-21)


### Bug Fixes

* Correct promql_metric_test_runner script name

## 1.1.0 (2025-03-20)


### Features

* Add promql metric validate test macro


### Bug Fixes

* Treat unrecognized flags to invokers as errors

## 1.0.2 (2025-03-18)


### Bug Fixes

* Move embedded bazelrc into a file

## 1.0.1 (2025-03-17)


### Bug Fixes

* Replace ghz_test_runner with nodocker/ghz_test_runner

## 1.0.0 (2025-03-16)


### BREAKING CHANGES

* Remove non-containerized version of CLI

## 0.28.0 (2025-03-10)


### Features

* Add --config-file flag to ghz_test_runner
* Add ghz_test_suite and ghz_test


### Bug Fixes

* ghz_test_runner should only set concurrency/total if no config file specified

## 0.27.1 (2025-03-03)


### Bug Fixes

* Adjust bazel args and gcloud creds secrets in Parc dockerfiles

## 0.27.0 (2025-02-26)


### Features

* Add promql to functionaltest-cli container image

## 0.26.0 (2025-02-24)


### Features

* Add --debug flag for deploy-and-test
* Call bazel clean unless in debug mode


### Bug Fixes

* Reduce size of functionaltest-cli stage
* Retain bazel test outputs.zip

## 0.25.0 (2025-02-20)


### Features

* remove build system version from functionaltest

## 0.24.0 (2025-02-19)


### Features

* Add output-dir flag
* Remove deprecated sut-dir flag

## 0.23.0 (2025-02-19)


### Features

* Add --init-bazel flag to download bazel
* Add Dockerfile-based build for functionaltest CLI
* Initialize bazel in functionaltest-cli docker image


### Bug Fixes

* Allow for TestRunner.Deploy to be unset

## 0.22.0 (2025-02-14)


### Features

* Add nodocker versions of rpc_diff_test runners


### Bug Fixes

* Check test-tools.tar exists before calling DockerLoad()
* Support shell command env vars
* Validate file path in DockerLoad()


### Dependencies

* **deps:** Upgrade rules_oci to 2.2.1
* **deps:** Use protobuf transitive deps

## 0.21.0 (2025-02-05)


### Features

* add test_size to test macros

## 0.20.0 (2025-01-29)


### Dependencies

* **deps:** Upgrade absl to e83ef27 2024-11-06
* **deps:** Upgrade bazel_skylib to 1.7.1
* **deps:** Upgrade build-system to 0.71.0
* **deps:** Upgrade container_structure_test to 1.19.3
* **deps:** Upgrade go toolchain to 1.23.4

## 0.19.1 (2024-09-27)


### Features

* Add client-type to rpc_diff_test

## 0.19.0 (2024-09-23)


### Features

* Add build-amazonlinux2023 to pull and push scripts
* Add filtered reply to test outputs
* Use Docker volumes instead of bind mounts in diff tests


### Bug Fixes

* Copy wrk2 request files to the container
* Load request files in the init function
* Remove WORKSPACE env var from wrk2_test_runner

## 0.18.0 (2024-07-02)


### Features

* Create tmp subdir in sut work dir

## 0.17.0 (2024-06-27)


### Features

* Allow gRPC tests to output stderr

## 0.16.1 (2024-06-21)


### Bug Fixes

* Simplify docker commands by mounting files

## 0.16.0 (2024-06-11)


### Bug Fixes

* Ensure docker load returns after load completes
* Ensure jq programs are not intepolated by shell


### Dependencies

* **deps:** Upgrade docker go package to v26.1.4

## 0.15.0 (2024-06-05)


### Features

* Add deps.bzl in sut workspace


### Bug Fixes

* Add tests for request pre-filtering
* Export all files in top-level sut package
* Move initialization of jq program variables
* SUT workspace should register jq toolchains

## 0.14.0 (2024-05-31)


### Bug Fixes

* Add internal README with installation instructions
* Add test-tools uri to bazel workspace status
* Add toolchains hash if running in build-system container
* Ensure tests/v2/run-tests generates test servers
* Invoke repos.bzl test_repositories explicitly
* Reorder sections in version config
* Run bazelisk with specified config
* Set bazelisk home within sut dir for hermeticity
* Support relative and absolute file paths
* Support use as submodule in get_workspace_status
* Update submodules
* Use dist tarball to docker load test servers image
* Use tests-tools uri in stable-status.txt


### Dependencies

* **deps:** Upgrade build-system to 0.64.1

## 0.13.0 (2024-05-23)


### Features

* Add container tests for grpc example servers
* Add docker-image flag to functionaltest cli
* Add go fmt to pre-commit
* Add run_all_tests bazel config
* Add target to copy functionaltest cli to dist dir
* Move test proto_descriptor_set targets to test package
* Update golang deps, switch to moby name for docker
* Upgrade bazel to 6.5.0


### Bug Fixes

* Add specified glob to warning message
* bazelignore test workspace
* **deps:** Replace rules_docker with rules_oci
* Enable C++ compiler warnings
* Move certificate-related tests to certs subdir
* Rewrite wrk2_test_runner for clarity
* Set default jq program to .
* Split single print statements into multiple, in lua scripts
* Use dist dir specific to tests/v1


### Documentation

* Generate docs for {deps,repos}.bzl
* Generate docs for public bzl files


### Dependencies

* **deps:** Upgrade aspect-build to v2.7.2
* **deps:** Upgrade bazel_skylib to v1.6.1
* **deps:** Upgrade build-system to 0.57.1
* **deps:** Upgrade build-system to 0.62.0
* **deps:** Upgrade build-system to v0.61.0
* **deps:** Upgrade build-system to v0.61.1
* **deps:** Upgrade rules_go to v0.47.1


### CLI

* **cli:** Add functionaltest CLI
* **cli:** Add functionaltest CLI version command
* **cli:** Add support for embedded bazel workspaces
* **cli:** Add support for invoking bazel
* **cli:** Add SUT deployment basics
* **cli:** Add top-level dockersut command
* **cli:** Add unzip golang lib
* **cli:** Add v2 tests
* **cli:** Embed functionaltest bazel apps

## 0.12.0 (2024-02-13)


### Features

* Allow relative file path references in wrk2 custom lua script

## 0.11.0 (2023-11-29)


### Features

* Add support for custom http path to diff and perf test macros
* Add support for tls to perf and diff test macros

## 0.10.0 (2023-11-20)


### Features

* Add wrk2 perf test macro
* Upgrade build-system to v0.50.0
* Use aspect_bazel_lib jq rule for perf macro pre processing

## 0.9.0 (2023-09-05)


### Features

* Allow for image tarballs to be supplied for custom rpc invokers

## 0.8.0 (2023-08-11)


### Features

* add support for custom rpc invokers for rpc diff testing
* Add sut.v1 proto package
* decouple rpc invokers from rpc diff test
* **deps:** Upgrade build-system to 0.42.0
* Enable buf lint COMMENTS rule category
* Support feat(deps) scope for release notes
* Upgrade base runtime docker image versions
* Upgrade build-system to release-0.28.0
* Upgrade build-system to v0.25.0


### Bug Fixes

* Allow for absolute path for rpc invoker app
* Upgrade build-system to 0.29.0


### Documentation

* Add separate doc to describe writing of test cases

## 0.7.0 (2023-05-04)


### Features

* Add pre-filter support for rpc perf tests


### Bug Fixes

* Avoid xtrace mode

## 0.6.0 (2023-05-04)


### Features

* Add bazel macro to generate CA bundle

## 0.5.1 (2023-05-03)


### Bug Fixes

* Ensure internal files use distinct filenames

## 0.5.0 (2023-05-03)


### Features

* Support SSL cert generation


### Bug Fixes

* Preclude wildcard matching for functional tests
* Refactor tests for clarity and simplicity

## 0.4.0 (2023-04-28)


### Features

* Add grpc envoy endpoint for tests
* Pretty-print all json responses
* Support curl for http rpc requests

## 0.3.0 (2023-04-24)


### Features

* Add docs for jq filter options
* Pass endpoint info directly to grpc_diff_test
* Remove irrelevant precommit hooks
* Upgrade build-system to release-0.23.0
* Upgrade to build-system 0.22.0


### Bug Fixes

* Remove docker containers after exit

## 0.2.0 (2023-04-03)


### Features

* Add test_tags to test suites
* Create annotated tag in addition to branch for releases
* Switch from cpu:arm64 to cpu:aarch64
* Upgrade build-system to release-0.21.1


### Bug Fixes

* Ensure changelog notes use specific version
* improve usage message for --endpoint-env-var flag in internal bazel grpcurl_diff_test_runner script
* Remove debug output
* Remove exit-status flag for post-filter jq

## 0.1.0 (2023-03-07)


### Features

* Add endpoint and rpc as grpcurl_diff_test_suite args
* add experimental script for generating performance perfgate benchmarks
* Add ghz load-testing support
* Add java formatter pre-commit hook
* Add local docker-based deployment and testing
* Add pre-rpc jq filter
* Add release scripts
* Add test for importing this repo through bazel workspace
* Add test_suites
* Basic grpc service diff testing
* Improve test file glob support in grpcurl_diff_test_suite
* Move glob for test files into BUILD from bzl function
* Reduce direct dependencies in the use_repo workspace
* refactors ab_to_perfgate_rundata to only output benchmark key in generated quickstore input file
* Support jq --slurp filters
* Update to build-system 0.14.0
* Upgrade black to 23.1.0
* Upgrade build-system to 0.21.0
* Upgrade build-system to release-0.18.0
* Upgrade build-system to release-0.20.0
* Upgrade build-system to v0.7.0
* Upgrade build-system to v0.8.0
* Upgrade to build-system 0.16.0
* Upgrade to build-system 0.17.0


### Bug Fixes

* Add @ for bazel repo
* Refactor all bazel third-party repositories into deps.bzl
* Require tools image env var TEST_TOOLS_IMAGE
* Use Label()


### Documentation

* Add Getting started to README
