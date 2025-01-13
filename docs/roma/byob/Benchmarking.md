# Roma Bring-Your-Own-Binary Microbenchmarking

Microbenchmark binaries can be found in [this folder](/src/roma/byob/sample_udf/). These include
microbenchmarks for "Hello World!", Sieve of Eratosthenes, sorting, and payloads for requests,
responses and callbacks.

## Running benchmarks

### Building the Docker tool:

The benchmarks can be used to determine suitable software configurations on given hardware. To build
the benchmarking tool, run the following command:

```sh
builders/tools/bazel-debian run //src/roma/byob/benchmark:copy_to_dist
```

### Choosing the correct image:

Running the command above, generates four tar archives for different configurations of the tool.
Namely,

-   `dist/roma_byob/benchmark_nonroot_image.tar`
-   `dist/roma_byob/benchmark_root_image.tar`
-   `dist/roma_byob/benchmark_nonroot_debug_image.tar`
-   `dist/roma_byob/benchmark_root_debug_image.tar`

Based on the choice of configuration, you can compare the numbers between gVisor and non-gVisor
mode. In non-gVisor mode, the gVisor layer is not used. The clone-pivot_root-exec construct is
utilized in both the gVisor and non-gVisor modes.

The remainder of this doc focuses on non-debug images. Debug images are largely the same image with
the addition of a shell. See
[distroless debug](https://github.com/GoogleContainerTools/distroless?tab=readme-ov-file#debug-images)
for more information.

The table below illustrates the docker flags needed to run the benchmarks.

<!-- prettier-ignore-start -->
<!-- markdownlint-disable line-length -->
| Tarball                     | Image Name and Tag                   | Supports non-Sandbox | `docker run` flags                                                     |
| --------------------------- | ------------------------------------ | -------------------- | ---------------------------------------------------------------------- |
| benchmark_nonroot_image.tar | privacy-sandbox/roma-byob/microbenchmarks:v1-nonroot | No                   | `--security-opt=seccomp=unconfined`                                    |
|                             |                                      |                      | `--security-opt=apparmor=unconfined`                                   |
|                             |                                      |                      | (optional)`--cap-add=CAP_SYS_ADMIN` only required for non-sandbox mode |
| benchmark_root_image.tar    | privacy-sandbox/roma-byob/microbenchmarks:v1-root    | Yes                  | Generally                                                              |
|                             |                                      |                      | `--security-opt=seccomp=unconfined`                                    |
|                             |                                      |                      | `--security-opt=apparmor=unconfined`                                   |
<!-- markdownlint-enable line-length -->
<!-- prettier-ignore-end -->

### Loading and running the tool (root image)

After building, load the tool into docker as follows:

```sh
docker load -i dist/roma_byob/benchmark_root_image.tar
```

To run the tool and benchmark BYOB locally:

```sh
docker run -it \
  --security-opt=seccomp=unconfined \
  --security-opt=apparmor=unconfined \
  --cap-add=CAP_SYS_ADMIN \
  --rm \
  privacy-sandbox/roma-byob/microbenchmarks:v1-root
```

By default, the results are printed to the console. If you want to store the results in a file,
define an environment variable called `BENCHMARK_OUT` pointing to the json file where you want to
store the results. For easy access to the benchmark output file, mount a writeable directory into
the container and set the container's `BENCHMARK_OUT` environment variable to a file within that
target directory. For example:

```sh
benchmark_out_dir="/benchmark_data"
# /path/to/directory is a writable directory
docker run \
  -v /path/to/directory:${benchmark_out_dir} \
  "--env=BENCHMARK_OUT=${benchmark_out_dir}/roma_byob_benchmark.json" \
  -it \
  --security-opt=seccomp=unconfined \
  --security-opt=apparmor=unconfined \
  --cap-add=CAP_SYS_ADMIN \
  --rm \
  privacy-sandbox/roma-byob/microbenchmarks:v1-root
```

### Benchmarking on AWS EC2

The following section covers how this image can be converted to an EIF and uploaded to an EC2
instance for benchmarking in AWS Nitro Enclave.

To build and upload the EIF to EC2 instance:

```sh
AWS_EC2_ADDR=...
AWS_EC2_PEM=...
builders/tools/bazel-debian run //src/roma/byob/benchmark:copy_to_dist
builders/tools/convert-docker-to-nitro \
  --builder-image build-amazonlinux2023 \
  --docker-image-tar dist/roma_byob/benchmark_root_image.tar \
  --docker-image-uri roma_byob_benchmark_image \
  --docker-image-tag v1-root \
  --outdir dist/roma_byob/ \
  --eif-name roma_byob_benchmark_image
scp -i "${AWS_EC2_PEM}" \
  dist/roma_byob/roma_byob_benchmark_image.eif \
  ec2-user@${AWS_EC2_ADDR}:/home/ec2-user/
```

The following example executes the benchmarks in a Nitro Enclave TEE from your EC2 instance (Note:
adjust CPU and memory as desired):

```sh
nitro-cli run-enclave --cpu-count 2 --memory 1684 --eif-path roma_byob_benchmark_image.eif --enclave-cid 10 --attach-console
```

## Benchmark results

Running the benchmarks on an ARM64 machine with `8 X 50 MHz CPUs` yielded the following results:

| Benchmark                                               | Time    | CPU      | Iterations | Details         |
| ------------------------------------------------------- | ------- | -------- | ---------- | --------------- |
| BM_ProcessRequestPrimeSieve/mode:0/prime_count:100000   | 4.37 ms | 0.067 ms | 1000       | mode:gVisor     |
| BM_ProcessRequestPrimeSieve/mode:1/prime_count:100000   | 15.7 ms | 0.071 ms | 1000       | mode:Non-gVisor |
| BM_ProcessRequestPrimeSieve/mode:0/prime_count:500000   | 5.20 ms | 0.071 ms | 1000       | mode:gVisor     |
| BM_ProcessRequestPrimeSieve/mode:1/prime_count:500000   | 15.6 ms | 0.078 ms | 1000       | mode:Non-gVisor |
| BM_ProcessRequestPrimeSieve/mode:0/prime_count:1000000  | 5.69 ms | 0.071 ms | 1000       | mode:gVisor     |
| BM_ProcessRequestPrimeSieve/mode:1/prime_count:1000000  | 15.7 ms | 0.084 ms | 1000       | mode:Non-gVisor |
| BM_ProcessRequestPrimeSieve/mode:0/prime_count:5000000  | 18.3 ms | 0.085 ms | 1000       | mode:gVisor     |
| BM_ProcessRequestPrimeSieve/mode:1/prime_count:5000000  | 17.7 ms | 0.073 ms | 1000       | mode:Non-gVisor |
| BM_ProcessRequestPrimeSieve/mode:0/prime_count:10000000 | 37.4 ms | 0.094 ms | 100        | mode:gVisor     |
| BM_ProcessRequestPrimeSieve/mode:1/prime_count:10000000 | 36.7 ms | 0.091 ms | 100        | mode:Non-gVisor |
| BM_ProcessRequestSortList/mode:0/n_items:10000          | 4.60 ms | 0.065 ms | 1000       | mode:gVisor     |
| BM_ProcessRequestSortList/mode:1/n_items:10000          | 15.6 ms | 0.072 ms | 1000       | mode:Non-gVisor |
| BM_ProcessRequestSortList/mode:0/n_items:100000         | 7.05 ms | 0.071 ms | 1000       | mode:gVisor     |
| BM_ProcessRequestSortList/mode:1/n_items:100000         | 15.7 ms | 0.073 ms | 1000       | mode:Non-gVisor |
| BM_ProcessRequestSortList/mode:0/n_items:1000000        | 64.6 ms | 0.094 ms | 100        | mode:gVisor     |
| BM_ProcessRequestSortList/mode:1/n_items:1000000        | 64.9 ms | 0.096 ms | 100        | mode:Non-gVisor |

These results include only some of the benchmarks run. For all benchmarks, consider running them
yourself. Refer also to the source code for the benchmarks in
[roma_byob_benchmark.cc](/src/roma/byob/benchmark/roma_byob_benchmark.cc).
