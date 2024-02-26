# Logging In Roma

Roma provides built-in logging capabilities to help you debug, monitor, and maintain your JavaScript
and WebAssembly (WASM) User-Defined Functions (UDFs). This is particularly useful when you are
debugging or writing complex UDFs.

## Using the console

The console can be used to log from UDFs. Support within Roma has been provided for the following
logging functions.

-   `console.log()`:
    -   Logs an informational message.
    -   Use this for general information about the progress of your UDF execution.
-   `console.warn()`:
    -   Logs a warning message.
    -   Use this for potential problems or unexpected behavior that does not immediately halt UDF
        execution.
-   `console.error()`:
    -   Logs an error message.
    -   Use this for critical issues that prevent your UDF from completing successfully.

## Performance

| Benchmark                   | Time     | CPU      | Log Message Size | Num Log Calls |
| --------------------------- | -------- | -------- | ---------------- | ------------- |
| BM_RomaLogging/1/1          | 0.993 ms | 0.021 ms | 1                | 1             |
| BM_RomaLogging/10/1         | 1.35 ms  | 0.023 ms | 1                | 10            |
| BM_RomaLogging/100/1        | 4.61 ms  | 0.031 ms | 1                | 100           |
| BM_RomaLogging/1000/1       | 35.1 ms  | 0.025 ms | 1                | 1000          |
| BM_RomaLogging/10000/1      | 323 ms   | 0.057 ms | 1                | 10000         |
| BM_RomaLogging/1/10         | 0.954 ms | 0.022 ms | 10               | 1             |
| BM_RomaLogging/10/10        | 1.41 ms  | 0.023 ms | 10               | 10            |
| BM_RomaLogging/100/10       | 4.75 ms  | 0.025 ms | 10               | 100           |
| BM_RomaLogging/1000/10      | 36.3 ms  | 0.042 ms | 10               | 1000          |
| BM_RomaLogging/10000/10     | 332 ms   | 0.040 ms | 10               | 10000         |
| BM_RomaLogging/1/100        | 0.938 ms | 0.022 ms | 100              | 1             |
| BM_RomaLogging/10/100       | 1.36 ms  | 0.023 ms | 100              | 10            |
| BM_RomaLogging/100/100      | 4.97 ms  | 0.037 ms | 100              | 100           |
| BM_RomaLogging/1000/100     | 37.2 ms  | 0.036 ms | 100              | 1000          |
| BM_RomaLogging/10000/100    | 358 ms   | 0.058 ms | 100              | 10000         |
| BM_RomaLogging/1/1000       | 0.956 ms | 0.021 ms | 1000             | 1             |
| BM_RomaLogging/10/1000      | 1.42 ms  | 0.023 ms | 1000             | 10            |
| BM_RomaLogging/100/1000     | 5.72 ms  | 0.041 ms | 1000             | 100           |
| BM_RomaLogging/1000/1000    | 48.0 ms  | 0.043 ms | 1000             | 1000          |
| BM_RomaLogging/10000/1000   | 475 ms   | 0.061 ms | 1000             | 10000         |
| BM_RomaLogging/1/10000      | 0.964 ms | 0.022 ms | 10000            | 1             |
| BM_RomaLogging/10/10000     | 1.54 ms  | 0.023 ms | 10000            | 10            |
| BM_RomaLogging/100/10000    | 6.69 ms  | 0.036 ms | 10000            | 100           |
| BM_RomaLogging/1000/10000   | 58.2 ms  | 0.049 ms | 10000            | 1000          |
| BM_RomaLogging/10000/10000  | 573 ms   | 0.048 ms | 10000            | 10000         |
| BM_RomaLogging/1/100000     | 1.28 ms  | 0.017 ms | 100000           | 1             |
| BM_RomaLogging/10/100000    | 4.63 ms  | 0.030 ms | 100000           | 10            |
| BM_RomaLogging/100/100000   | 36.9 ms  | 0.047 ms | 100000           | 100           |
| BM_RomaLogging/1000/100000  | 353 ms   | 0.054 ms | 100000           | 1000          |
| BM_RomaLogging/10000/100000 | 3461 ms  | 0.068 ms | 100000           | 10000         |

This benchmark measures the time it takes to execute a simple UDF that calls `console.log()` with a
`N` character long string `M` times. Benchmark code can be found
[here](/src/roma/benchmark/logging_benchmark.cc).

## Example

```js
function myUDF(input) {
    // ... UDF logic

    console.log('Processing input: ' + input); // Informational log

    if (someCondition) {
        // Warning log
        console.warn('Potential issue detected in input: ' + input);
    }

    try {
        // ... (code that could potentially throw an error)
    } catch (error) {
        // Error log
        console.error('Error encountered: ' + error.message);
    }
}
```
