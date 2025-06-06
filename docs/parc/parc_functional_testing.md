# Parc functional tests

This document gives an overview of the functional testing system used for Parc. The Parc server has
reference implementations for GCP, local, AWS, and Azure. Of these, there are functional tests for
GCP, local, and Azure.

The tests use the [functionaltest-system CLI](/testing/functionaltest-system/Dockerfile) to run the
tests. The test cases and other data are first compiled into a zip archive passed to the functional
test CLI, which then runs the tests.

The tests can be found in the [testing/parc/](/testing/parc/) directory of the common repo.

While there are differences for each platform, in general the tests follow these steps:

1.  Building an image of the Parc server
1.  Building an image of the functional test CLI
1.  Compiling a zip archive of the test cases and other test data (system-under-test \[SUT\] data)
1.  Running the test cases in the SUT data using the functional test CLI

This document provides a summary of how the functional test CLI works, and then a description of the
various Parc functional test implementations.

[TOC]

## The Functional Test CLI

The functional test CLI is used mainly to run diff testing and load testing of gRPC or HTTP
endpoints. For instructions building the CLI, see the
[README.md](/testing/functionaltest-system/README.md). The main runtime is defined in the
`functest-cli` stage. A zip archive (the SUT archive) of the test cases must be compiled and mounted
into the CLI container.

The CLI executes various bazel or shell commands stages defined by a
[`TestRunner`](/testing/functionaltest-system/sut/v1/sut.proto) proto. Shell commands are executed
directly in the CLI runtime. Bazel stages build and run a target defined in a `BUILD.sut` which is
compiled into the SUT data zip archive. If any stage exits with a non-zero error code, the CLI exits
early with an error status. An example `TestRunner` could look like this:

```proto
name: "parc-azure test-sut"

stages: {
  command: {
    shell: {
      command: "/populate/populate_azurite"
      env {
        key: "AZURE_STORAGE_CONNECTION_STRING"
        value: "DefaultEndpointsProtocol=http;AccountName=devstoreaccount1;AccountKey=Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/k1SZFPTOtr/KBHBeksoGMGw==;BlobEndpoint=http://azurite-service:10000/devstoreaccount1;"
      }
    }
  }
}
stages: {
  command: {
    bazel: {
      test_targets: "//test_cases:all_tests"
    }
  }
}
```

The above test run consists of two stages, the first being a shell command to populate a storage
server with test blobs, and the second a `bazel run` command of the `//test_cases:all_tests` target.

### The SUT archive

This zip archive contains all the various test cases to run, test data such as `.pb` descriptor
files, and, crucially, a `BUILD.sut` file which defines the tests themselves. The SUT archive is
compiled from the SUT directory, which is the directory containing a `BUILD.bazel` file defining a
[`sut` bazel rule](/testing/functionaltest-system/sut/sut.bzl). This rule copies over the SUT
directory and all its children, some exclusions (noted in the `sut` rule definition). Note that
because of this, files in the SUT dir must be arranged as such:

```text
testing/parc/azure/local-docker-compose-hermetic/
|-- BUILD.bazel <-- contains a `sut` rule, defining this as the SUT root dir
|-- (other test files)
|-- test_cases
|   |-- BUILD.sut <-- defines the various tests
|   |-- metric_validation.bzl <-- additional bazel rule file, in this case for metrics tests
|   |-- privacysandbox.apis.parc.v0.ParcService.GetBlob <-- test cases for GetBlob rpc defined here
|   |   |-- error-empty.reply.json <-- expected response for "error-empty" test
|   |   |-- error-empty.request.json <-- request sent for "error-empty" test
|   |   |-- full-blob.filter.slurp.jq <-- jq filter applied to "full-blob" test response
|   |   |-- full-blob.pre-filter.jq <-- jq filter applied to "full-blob" test request
|   |   |-- full-blob.reply.json <-- expected response for "full-blob" test
|   |    -- full-blob.request.json <-- request for "full-blob" test
|    -- privacysandbox.apis.parc.v0.ParcService.ListBlobsMetadata <-- test cases for other RPC
 -- testrunner.textproto <-- defines `TestRunner` stages, must be defined at root of SUT dir
```

An example `BUILD.sut` can be found
[here](/testing/parc/azure/local-k8s-hermetic/test_cases/BUILD.sut). An example `BUILD.bazel file
which compiles an SUT archive can be found
[here](/testing/parc/azure/local-k8s-hermetic/BUILD.bazel).

These test cases consist of a request, a jq filter applied to the request prior to sending it
(optional), a jq filter applied to the response after receiving it (optional), and an expected
response.

A test is defined as a test case along with the test type (diff or load) along with other
configuration such as endpoint hostname and port or the descriptor set `.pb` files for a gRPC
request. The various tests can be defined as a group using a test suite (e.g. `rpc_diff_test_suite`
or `ghz_test_suite`, defined [here](/testing/functionaltest-system/bazel/rpc_test.bzl)).

### Test case format

A test case is defined as a request and response, along with optional pre and post jq filters. The
pre filter is applied to the request prior to sending, and the post filter is applied to the
received response prior to diffing against the expected request. The files defining these must share
a test case name. Files must have the following format:

| File type           | Filename format             |
| :------------------ | :-------------------------- |
| Request payload     | `{test_name}.request.json`  |
| Response payload    | `{test_name}.reply.json`    |
| Request pre-filter  | `{test_name}.pre-filter.jq` |
| Request post-filter | `{test_name}.filter.jq`     |

Note that post filter jq files can have the name `{test_name}.filter.slurp.jq` which will combine a
stream of RPC responses into a single JSON array.

### SUT_ID env var

The functional test CLI creates a unique identifier for each test run, which is passed to each test
stage as an environment variable called `SUT_ID`. This is provided to the JQ filter commands as a
local variable, `$SUT_ID`. Because this ID is available in all stages, programmatically-named test
data can use this ID, and the test cases can modify the requests and expected responses to use this
ID.

## GCP functional tests

There is one, nonhermetic version of the Parc GCP functional tests. The SUT dir can be found
[here](/testing/parc/gcp/nonhermetic), with `run_tests` script being the main entrypoint. This
entrypoint builds the Parc GCP server and the functional test CLI and then runs both on a Docker
network managed with Docker compose
([`docker-compose.yaml`](/testing/parc/gcp/nonhermetic/docker-compose.yaml)).

These tests are nonhermetic because they retrieve blobs from production GCS and Secret Manager. The
blobs and secrets are in the `kiwi-air-force-remote-build` GCP project. The GCS Storage account can
be found
[here](https://pantheon.corp.google.com/storage/browser?inv=1&invt=AbyErA&project=kiwi-air-force-remote-build),
with the tests checking the blobs in the `parc-functional-testing` bucket.

These tests are run with the default GCloud system credentials, so you must be logged-in locally and
have access to the `kiwi-air-force-remote-build` project.

The GCP tests have a single stage which runs all of the test cases as both perf and diff tests.

## Local functional tests

There is one, hermetic version of the Parc local server functional tests. The SUT dir can be found
[here](/testing/parc/local/docker-compose-hermetic), with `run-test` script being the main
entrypoint. This entrypoint builds the Parc local server and the functional test CLI.

This test populates a local directory, then mounts this directory into the Parc server container,
which is used for local blob data retrieval.

The `run-test` script then runs the Parc server and the functional test CLI on a Docker network
managed with Docker compose
([`docker-compose.yaml`](/testing/parc/local/docker-compose-hermetic/docker-compose.yaml)).

The local tests have a single stage which runs all of the test cases as both perf and diff tests.

## Parc Azure functional tests

There are three flavors of Parc functional tests:

1.  Local hermetic testing on a docker-compose network
1.  Local hermetic testing using [kind](https://kind.sigs.k8s.io/)
1.  Hermetic testing on Azure Kubernetes cluster,
    [`parc-aks-cluster`](https://portal.azure.com/#@privacysandbox.onmicrosoft.com/resource/subscriptions/5f35a170-558f-4685-99a4-aa9014412fca/resourceGroups/parc-rg/providers/Microsoft.ContainerService/managedClusters/parc-aks-cluster/overview)

The three tests range from a simpler environment for dev purposes (local docker compose) to a
production environment using Azure storage. To reduce duplication, the tests all use the same test
cases where possible.

### Generating Test Data

In order to maintain hermeticity, the Azure tests generate different containers named using the
run-specific `SUT_ID` env var. As such test cases must account for this using appropriate pre and
post filters.

The local docker compose and kind tests share the
[`populate_azurite`](/testing/parc/azure/local-docker-compose-hermetic/populate_azurite) script to
populate the
[Azurite](https://learn.microsoft.com/en-us/azure/storage/common/storage-use-azurite?tabs=visual-studio%2Cblob-storage)
test server. The `test_blob.bin` file is shared between the three test versions.

The storage commands are executed using the
[Azure CLI](https://learn.microsoft.com/en-us/cli/azure/?view=azure-cli-latest), which is included
in the functional test CLI. These local tests authenticate using the
`AZURE_STORAGE_CONNECTION_STRING` env var, which is passed to the functional test CLI as part of the
`TestRunner` stages.

The cloud tests, #azure-k8s-hermetic, use a slightly modified version of the script found
[here](/testing/parc/azure/azure-k8s-hermetic/helmchart/sutdata/populate_azurite). This uses the
Workload Identity auth provided by the `parc-aks-cluster` to write to the `parcsa` Storage Account
on Azure. This necesitates a cleanup script to delete these blobs when the test ends, which can be
found [here](/testing/parc/azure/azure-k8s-hermetic/helmchart/sutdata/delete_container).

### `local-docker-compose-hermetic`

The SUT dir can be found [here](/testing/parc/azure/local-docker-compose-hermetic). The main
entrypoint for this is [`run-test`](/testing/parc/azure/local-docker-compose-hermetic/run-test)
which builds the Parc server and the functional test CLI. These are run on a docker compose network
([`docker-compose.yaml`](/testing/parc/azure/local-docker-compose-hermetic/docker-compose.yaml))
alongside an [OTel collector](https://opentelemetry.io/docs/collector/) and a
[Prometheus server](https://prometheus.io/docs/prometheus/latest/querying/basics/) to allow for
metric querying.

An Azurite test server is used as a local storage emulator for Azure Storage. The Parc server
authenticates with the Azurite server using
[Shared Key Auth](https://learn.microsoft.com/en-us/rest/api/storageservices/authorize-with-shared-key)
for the `devstoreaccount1` default dev account. These are provided to the server as `--account-name`
and `--account-key`, with the values coming from environment variables defined in the compose env.

The tests have the following stages:

1.  Populate Azurite
1.  Run test cases
1.  Sleep to allow OTel metrics to be collected
1.  Querying for parc stats from the Prometheus server
1.  Validating metrics such as error rate

### `local-k8s-hermetic`

The SUT dir can be found [here](/testing/parc/azure/local-k8s-hermetic). These are run in a local
kubernetes cluster ([`test_cluster.yaml`](/testing/parc/azure/local-k8s-hermetic/test_cluster.yaml))
alongside an [OTel collector](https://opentelemetry.io/docs/collector/) and a
[Prometheus server](https://prometheus.io/docs/prometheus/latest/querying/basics/) to allow for
metric querying.

An Azurite test server is used as a local storage emulator for Azure Storage. The Parc server
authenticates with the Azurite server using
[Shared Key Auth](https://learn.microsoft.com/en-us/rest/api/storageservices/authorize-with-shared-key)
for the `devstoreaccount1` default dev account. These are provided to the server as `--account-name`
and `--account-key`, with the values coming from environment variables defined
`parc_deployment.yaml`.

The main entrypoint for these tests is
[`create-cluster-run-test`](/testing/parc/azure/local-k8s-hermetic/create-cluster-run-test). This
script manages the creating and deleting the local Kubernetes cluster, while `run-test` manages
deploying the Kubernetes workload to the cluster. `run-test` creates the workload YAML files using
helm, and runs the `kubectl apply` command to deploy the workload. See #kubernetes-workload for more
info.

The tests have the following stages:

1.  Populate Azurite
1.  Run test cases
1.  Sleep to allow OTel metrics to be collected
1.  Querying for parc stats from the Prometheus server
1.  Validating metrics such as error rate

### `azure-k8s-hermetic`

These tests run on Azure. In addition to having an Azure Account set up, the following must also be
set up:

1.  You are logged in to Azure Container Registry. Follow
    [these steps](https://docs.google.com/document/d/1mc2CVm6OHXplKlTuUyTZ7WZptPpmQjhdwfFc8F7Q9Pc/edit?tab=t.0#heading=h.bvupj3uyuxp1)
    if you do not have the registry credentials.
1.  You have the `parc-aks-cluster` credentials locally. Use this command to add the credentials:
    `az aks get-credentials --resource-group parc-rg --name parc-aks-cluster`

The SUT dir can be found [here](/testing/parc/azure/azure-k8s-hermetic). These are run in an Azure
[AKS](https://azure.microsoft.com/en-us/products/kubernetes-service) cluster Kubernetes cluster
([`parc-aks-cluster`](https://portal.azure.com/#@privacysandbox.onmicrosoft.com/resource/subscriptions/5f35a170-558f-4685-99a4-aa9014412fca/resourceGroups/parc-rg/providers/Microsoft.ContainerService/managedClusters/parc-aks-cluster/overview))
alongside an [OTel collector](https://opentelemetry.io/docs/collector/) and a
[Prometheus server](https://prometheus.io/docs/prometheus/latest/querying/basics/) to allow for
metric querying.

The Azure tests retrieve data from
[Azure Blob Storage](https://azure.microsoft.com/en-us/products/storage/blobs). The data is created
in generated test container in the `parcsa`
[Storage Account](https://learn.microsoft.com/en-us/azure/storage/common/storage-account-overview).
The container and blobs are created using the Azure CLI, which runs in the `TestRunner` stages in
the `functest` job. The `populate_azurite` and `delete_container` scripts are used for container
creation and deletion respectively.

The `azure-k8s-hermetic` workload relies on
[Workload Identity](https://learn.microsoft.com/en-us/azure/aks/workload-identity-overview?tabs=dotnet)
authentication to interact with Azure services. This works by injecting environment variables in
eligible containers which are used for authentication by the Azure CLI and the Azure C++ SDK.

The `parc-aks-cluster` is configured to use this auth method which provides auth tokens through the
enabled [OIDC issuer URL](https://learn.microsoft.com/en-us/azure/aks/use-oidc-issuer). This access
is mediated through the `parc-aks-user-identity`
[User-Assigned Managed Identity](https://learn.microsoft.com/en-us/entra/identity/managed-identities-azure-resources/overview)
which has the `Storage Blob Data Owner` role. The last piece of infrastructure is required for auth
is a
[federated credential](https://azure.github.io/azure-workload-identity/docs/topics/federated-identity-credential.html),
which is created per-run due to the namespace-specific nature of the credential (each run occurs in
a unique Kubernetes namespace). The creation and deletion of federated credentials can be found in
the `run-test` script.

Aside from the above-mentioned infra components, the deployed Kubernetes workload must contain the
following attributes: 1. A `ServiceAccount` object which references the Managed Identity and
Kubernetest Namespace 1. The `functest` `Job` and `parc-server` `Deployment` must be associated with
the above `ServiceAccount` and must have `azure.workload.identity/use: 'true'`

The main entrypoint for these tests is
[`deploy-to-azure-run-test`](/testing/parc/azure/azure-k8s-hermetic/deploy-to-azure-run-test). This
script manages the creating and deleting the local Kubernetes cluster, while `run-test` manages
deploying the Kubernetes workload to the cluster.

`run-test` creates the workload YAML files using helm, and runs the `kubectl apply` command to
deploy the workload. See #kubernetes-workload for more info.

The tests have the following stages:

1.  Populate Azure Blob Storage
1.  Run test cases
1.  Sleep to allow OTel metrics to be collected
1.  Querying for parc stats from the Prometheus server
1.  Validating metrics such as error rate
1.  Delete test container in Azure Blob Storage

### Kubernetes Workload

The `local-k8s-hermetic` and `azure-k8s-hermetic` tests both use [`helm`](https://helm.sh/) to
generate a Kubernetes workload YAML file from YAML templates, collectively referred to as a
"helmchart".

The helmchart can be found in the `helmchart/` dir of the SUT dir for each test flavor.

The overall workload for both test flavors is very similar. Workload metadata can be found in
`Chart.yaml`, with shared values in `values.yaml`.

The `templates/` dir defines the jobs, deployments, services, namespace, and configmaps for the
workload. A single test run workload is all contained within a single namespace.

The `*-services.yaml` files all define `Service` objects, which are mainly used to provide routing
information between containers. The `*-deployment.yaml` and `*-job.yaml` define `Deployment` and
`Job` objects. Deployments are intended for containers which are intended to run until manually spun
down. Jobs are intended for containers which run once and exit.

Overall, the deployment workload mirrors the simpler docker compose environment:

-   Parc server
-   OTel collector
-   Prometheus server
-   Functional test CLI
-   Azurite server (not present in `azure-k8s-hermetic`)

The Kubernetes workload configs capture some of the dependencies between certain jobs by using
`initContainers`, which run sequentially before the ones in the `containers` section (which run in
parallel) to wait for all the services to be ready before running the test.

The main test driver remains the functional test CLI container, which is managed by
`functest-job.yaml`.

#### ConfigMaps

A key difference between the docker compose environments and the Kubernetes environments is the use
of [`ConfigMaps`](https://kubernetes.io/docs/concepts/configuration/configmap/). The functional
tests primarily use these to provide volume mounts for adding data to and retrieving output from the
container environments.

The three `ConfigMaps` in the workload are:

1.  `configs`: provides config YAMLs for the OTel collector and Prometheus server
1.  `parameters`: provides the `parameters.jsonl` file to the Parc server to populate `GetParameter`
    parameters
1.  `sutdata`: provides the SUT archive, along with `populate_azurite` and the `test_blob.bin`, all
    used in the `functest` Job

### Debugging Tips

#### Finding Test Outputs

All functional test flavors output to the `dist/tests/` directory (which is created if it does not
exist). This will contain the finalized `docker-compose.yaml` or `Workload.yaml` file, which should
always be created. An early exit here is usually caused by misconfiguration in the docker compose or
cluster config. For docker compose it might be useful to run the containers individually. For
Kubernetes, it might be useful to run:

`kubectl --context={cluster name} --namespace={namespace name} describe pod {pod name}`

The cluster, namespace, and pod identifiers should be in the test output. The `process_job` function
demonstrates a way of getting the pod name from the job name.

The functional test container will generate most of the useful test output. Ensure that the
`--output-dir` flag is set to a mounted directory which can be inspected once the container exits.

Each stage's logs will be captured in a different zip file for that stage (e.g. `stage1-logs.zip`).
Each test case will be listed in the `test_cases/` directory of this zip file.

#### Debugging `azure-k8s-hermetic` test case failures

Test failures from the `functest` Job on the `parc-aks-cluster` can be difficult to debug because
the outputs are not local or persisted.

To get around this you can change the `functest` job container like so:

```shell
          command: ["/bin/sh", "-c", "sleep 3600s"]
          # args:
          #   - dockersut
          #   - deploy-and-test
          #   - --sut-zip
          #   - /sutdata/sut_parc.zip
```

This will keep the container up for 1 hr once you run `deploy-to-azure-run-test`. You can then run
the following command to open a shell into the `functest container`:
`kubectl exec --context=parc-aks-cluster -n {NAMESPACE NAME} -it {POD NAME} -c functest -- /bin/bash`

Once inside, you can run the functional test CLI manually using the following command:
`/usr/local/bin/functionaltest dockersut deploy-and-test --sut-zip /sutdata/sut_parc.zip --output-dir /some_dir`

From there you should have the test outputs in `/some_dir` to see stage failures.

#### Isolating tests

Most of the time you will want to check failures coming from diff tests. Load tests do not check
response payloads, so if load tests are failing, likely the functional test container cannot reach
the Parc server, which would point to an issue in the compose or cluster config.

To isolate a single RPC, modify the `BUILD.sut` file in the following way:

1.  Remove `rpcs` from the list in the `endpoint` to only have the rpc you want to look at
1.  Change `("diff", "ghz")` in `test_suite` to `("diff",)` to only run diff tests (Mind the
    trailing comma!)

In addition, you probably only want to run the stage with the test cases. Modify the
`testrunner.textproto` to only include the stage containing the test cases (be sure to leave the
`populate_azurite` stage though!).

Lastly, some tests errors may cause the functional test CLI container to become unresponsive. The
default is to run the functest pod for 10 minutes. You can lower this to `2 minutes` in the
`run-test` script in order to run the tests for a shorter duration. Alternatively, `docker ps` and
`docker kill` can be used to stop the tool.

#### Modifying test runner scripts

The functional test CLI delegates sending gRPC requests to the
[`bazel/*_runner`](/testing/functionaltest-system/bazel/rpc_diff_test_runner) scripts. It can be
useful to modify these scripts locally to output more debug information, but be warned that ANY
extra output from the script will cause a test failure.
