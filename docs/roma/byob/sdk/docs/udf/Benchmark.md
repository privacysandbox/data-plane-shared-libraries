# Roma Bring-Your-Own-Binary Benchmark

For details about Roma Bring-Your-Own-Binary (BYOB), check out the
[top-level document](/docs/roma/byob/sdk/docs/Guide%20to%20the%20SDK.md).

Benchmark binaries can be found in [this folder](/src/roma/byob/udf/). This includes benchmarks for
"Hello World!", Sieve of Eratosthenes, sorting, and payloads for requests, responses and callbacks.

## Running benchmarks

The benchmarks can be used to determine suitable software configurations on given hardware.

```sh
builders/tools/bazel-debian run //src/roma/byob/benchmark:copy_to_dist
```

After building, load the tool into docker as follows:

```sh
docker load -i dist/roma_byob/benchmark_root_image.tar
```

To run the tool and benchmark BYOB locally:

```sh
docker run -it --privileged --rm roma_byob_benchmark_image:v1-root
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

To run the benchmarks in a Nitro Enclave TEE:

```sh
nitro-cli run-enclave --cpu-count 2 --memory 1684 --eif-path roma_byob_benchmark_image.eif --enclave-cid 10 --attach-console
```

## Benchmark results

Running the benchmarks on an ARM64 machine with `8 X 50 MHz CPUs` yielded the following results:

| Benchmark                                              | Time    | CPU      | Iterations | Details          |
| ------------------------------------------------------ | ------- | -------- | ---------- | ---------------- |
| BM_ExecuteBinaryPrimeSieve/mode:0/prime_count:100000   | 4.37 ms | 0.067 ms | 1000       | mode:Sandbox     |
| BM_ExecuteBinaryPrimeSieve/mode:1/prime_count:100000   | 15.7 ms | 0.071 ms | 1000       | mode:Non-Sandbox |
| BM_ExecuteBinaryPrimeSieve/mode:0/prime_count:500000   | 5.20 ms | 0.071 ms | 1000       | mode:Sandbox     |
| BM_ExecuteBinaryPrimeSieve/mode:1/prime_count:500000   | 15.6 ms | 0.078 ms | 1000       | mode:Non-Sandbox |
| BM_ExecuteBinaryPrimeSieve/mode:0/prime_count:1000000  | 5.69 ms | 0.071 ms | 1000       | mode:Sandbox     |
| BM_ExecuteBinaryPrimeSieve/mode:1/prime_count:1000000  | 15.7 ms | 0.084 ms | 1000       | mode:Non-Sandbox |
| BM_ExecuteBinaryPrimeSieve/mode:0/prime_count:5000000  | 18.3 ms | 0.085 ms | 1000       | mode:Sandbox     |
| BM_ExecuteBinaryPrimeSieve/mode:1/prime_count:5000000  | 17.7 ms | 0.073 ms | 1000       | mode:Non-Sandbox |
| BM_ExecuteBinaryPrimeSieve/mode:0/prime_count:10000000 | 37.4 ms | 0.094 ms | 100        | mode:Sandbox     |
| BM_ExecuteBinaryPrimeSieve/mode:1/prime_count:10000000 | 36.7 ms | 0.091 ms | 100        | mode:Non-Sandbox |
| BM_ExecuteBinarySortList/mode:0/n_items:10000          | 4.60 ms | 0.065 ms | 1000       | mode:Sandbox     |
| BM_ExecuteBinarySortList/mode:1/n_items:10000          | 15.6 ms | 0.072 ms | 1000       | mode:Non-Sandbox |
| BM_ExecuteBinarySortList/mode:0/n_items:100000         | 7.05 ms | 0.071 ms | 1000       | mode:Sandbox     |
| BM_ExecuteBinarySortList/mode:1/n_items:100000         | 15.7 ms | 0.073 ms | 1000       | mode:Non-Sandbox |
| BM_ExecuteBinarySortList/mode:0/n_items:1000000        | 64.6 ms | 0.094 ms | 100        | mode:Sandbox     |
| BM_ExecuteBinarySortList/mode:1/n_items:1000000        | 64.9 ms | 0.096 ms | 100        | mode:Non-Sandbox |

These results include only some of the benchmarks run. For all benchmarks, consider running them
yourself. Refer also to the source code for the benchmarks in
[roma_byob_benchmark.cc](/src/roma/byob/benchmark/roma_byob_benchmark.cc).
