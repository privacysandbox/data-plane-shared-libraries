# Traffic Generator Tool Documentation

## Overview

The `traffic_generator` tool is designed to simulate and measure traffic loads for Roma services,
supporting both BYOB and V8 execution modes. It allows users to test Roma services at scale,
enabling integrators to evaluate and optimize their Roma-based applications.

---

## Features

### Traffic Generation

-   **Burst Generation**: Sends bursts of requests at configurable intervals and sizes.
-   **Configurable Parameters**:
    -   `queries_per_second`: Number of queries sent per second.
    -   `burst_size`: Number of requests per burst.
    -   `num_queries`: Total number of queries to send.
    -   `total_invocations`: Overrides `num_queries` if set; calculates queries as
        `total_invocations / burst_size`.
    -   `duration`: Optional duration to run the generator, overriding `num_queries`.

### Performance Measurements

-   **Latency Measurement**: Captures detailed latency statistics including min, p50, p90, p95, p99,
    and max latencies for:
    -   Executing a single invocation
    -   Executing a burst of invocations
    -   Processing a batch of invocations
-   **Failure and Late Burst Tracking**: Tracks invocation failures and late bursts, providing
    percentage metrics.

---

## Command-Line Flags

<!-- prettier-ignore-start -->
<!-- markdownlint-disable line-length -->
| Flag                      | Type               | Default Value        | Description                                                                 |
| ------------------------- | ------------------ | -------------------- | --------------------------------------------------------------------------- |
| `run_id`                  | `string`           | Arbitrary identifier | Identifier included in reports                                              |
| `num_workers`             | `int`              | 84                   | Number of pre-created workers                                               |
| `queries_per_second`      | `int`              | 42                   | Queries sent per second                                                     |
| `burst_size`              | `int`              | 14                   | Requests per burst                                                          |
| `num_queries`             | `int`              | 10,000               | Total queries to send                                                       |
| `total_invocations`       | `int`              | 0                    | Overrides `num_queries` if non-zero                                         |
| `sandbox`                 | `Mode`             | `nsjail`             | Sandbox mode (BYOB mode only)                                               |
| `syscall_filtering`       | `SyscallFiltering` | `untrusted`          | Syscall filtering level (BYOB mode only)                                    |
| `disable_ipc_namespace`   | `bool`             | true                 | Disable IPC namespace (BYOB mode only)                                      |
| `lib_mounts`              | `string`           | `/dir1,/dir2`        | Mount paths for pivot_root                                                  |
| `binary_path`             | `string`           | `/udf/sample_udf`    | Path to binary (BYOB mode)                                                  |
| `mode`                    | `string`           | `byob`               | Execution mode (`byob` or `v8`)                                             |
| `udf_path`                | `string`           |                      | Path to JavaScript UDF file (V8 mode only)                                  |
| `function_name`           | `string`           |                      | Handler function name (V8 Handler Function / BYOB Sample UDF Function)      |
| `input_args`              | `vector<string>`   |                      | Arguments for handler function. Number of primes if running BYOB PrimeSieve |
| `output_file`             | `string`           |                      | Output file path (JSON format)                                              |
| `verbose`                 | `bool`             | false                | Enable verbose logging                                                      |
| `sigpending`              | `optional<int>`    |                      | Pending signals rlimit                                                      |
| `duration`                | `absl::Duration`   | Infinite             | Duration to run generator                                                   |
| `find_max_qps`            | `bool`             | false                | Find max QPS maintaining performance                                        |
| `qps_search_bounds`       | `string`           | `1:10000`            | Lower and upper bounds for QPS search                                       |
| `late_threshold`          | `double`           | 15.0                 | Max acceptable percentage of late bursts                                    |
| `byob_connection_timeout` | `absl::Duration`   | 0                    | Max time to wait for a worker. (BYOB mode only)                             |
<!-- markdownlint-enable line-length -->
<!-- prettier-ignore-end -->

---

## Usage Examples

### Basic Execution (BYOB Mode)

```bash
./traffic_generator --mode=byob --binary_path=/path/to/binary --queries_per_second=100 --burst_size=10
```

### V8 Mode Execution with Profiling Enabled

```bash
./traffic_generator --mode=v8 --udf_path=/path/to/udf.js --handler_name=HandleFunc --enable_profilers=true --queries_per_second=100 --burst_size=10
```

---

## Output and Reporting

The tool generates detailed reports including:

-   Total runtime and invocation counts.
-   Failure and late burst percentages.
-   Latency statistics (min, p50, p90, p95, p99, max).
