# Roma V8 UDF Benchmarking

## Overview

The Roma Benchmark CLI tool provides functionality for User Defined Function (UDF) developers to
benchmark their code. It executes the UDF within the Roma V8 execution environment, providing
additional visibility to help developers better understand the runtime cost of their UDF. The
`RomaService::LoadCodeObj()` and `RomaService::Execute()` operations are independently benchmarked,
in the `BM_Load` and `BM_Execute` benchmarks respectively. The tool uses the google
microbenchmarking library and supports the standard set of benchmarking flags, such as
`--benchmark_time_unit` and `--benchmark_filter`. In addition to the benchmarking flags, the Roma
Benchmarking CLI includes command-line flags to specify the entrypoint JS function, the path to the
UDF to be benchmarked, and the path to a JSON file to be supplied to the CLI as input. Additionally,
the Roma benchmarking CLI supports prefixing the UDF with inline WASM generated as a result of the
`cc_inline_wasm_udf_js` build rule. `cc_inline_wasm_udf_js` generates a `.js` file, the path of
which can be supplied to the UDF Benchmarking CLI.

Run with: `builders/tools/bazel-debian run src/roma/tools/v8_cli:roma_benchmark -- <flags>`

## Flags

| Flag               | Description                        | Default Value |
| ------------------ | ---------------------------------- | ------------- |
| `--udf_file_path`  | Path to the UDF to be benchmarked. | `""` (empty)  |
| `--wasm_file_path` | Path to inline WASM.               | `""`(empty)   |
| `--entrypoint`     | Entrypoint JS Function.            | `""` (empty)  |
| `--input_json`     | Path to input JSON file.           | `""`(empty)   |
