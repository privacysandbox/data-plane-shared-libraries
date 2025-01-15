# Roma V8 Compiler Options

Roma provides the option to customize the optimizing compiler used by V8 for Js. V8 supports three
optimizing compilers, `--turbofan`, `--maglev`, and `--turboshaft` (default).

## Enabling a Compiler

To enable a given compiler, the `V8CompilerOptions` struct has been created. An instance of this
struct should be created and passed into `config.ConfigureV8Compilers()` to customize the optimizing
compilers used by V8.

```cpp
struct V8CompilerOptions {
  bool enable_turbofan = false;
  bool enable_maglev = false;
  bool enable_turboshaft = true;
};

V8CompilerOptions opts = { .enable_maglev = true };
Config config;
config.ConfigureV8Compilers(opts);
```

## Compilers

### Turbofan

Turbofan is V8's optimizing compiler focused on peak performance. See more information about
Turbofan [here](https://v8.dev/blog/turbofan-jit)

### Maglev

Maglev is V8's mid tier optimizing compiler, sitting in between Sparkplug (V8's base compiler) and
Turbofan. See more information about Maglev [here](https://v8.dev/blog/maglev)

### Turboshaft

Turboshaft is new internal architecture for Turbofan, allowing for new optimizations and faster
compilation. Turboshaft is the default optimizing compiler used within Roma. See more information
about Turboshaft [here](https://v8.dev/blog/holiday-season-2023)

## Performance Impact

The impact of a given combination of optimizing compilers varies depending on UDF size and
complexity. Integrators are recommended to conduct their own benchmarks with an appropriately sized
UDF, and adjust compiler options based on their results. Some preliminary benchmarks have already
been found, measuring the performance of loading and executing UDFs of various sizes and
complexities. Below are the 5 most performant flag combinations, with the average % improvement
across all UDFs and highest % improvement for a single UDF.

[Benchmark Code](/src/roma/benchmark/compiler/execute_benchmark.cc)

[Benchmarks](https://docs.google.com/spreadsheets/d/1Ynccs3yRG6eYZeJPJNqYkL8cBfverSND74eu0rMBVDQ/edit?resourcekey=0-mOjacqxNHadR6ZzVYAMThQ#gid=525637620)

| Flag                             | Average % Improvement\* | Max % Improvement - Load | Max % Improvement - Execute |
| -------------------------------- | ----------------------- | ------------------------ | --------------------------- |
| --turboshaft                     | 2.8                     | 13.3                     | 6.1                         |
| --turbofan --maglev --turboshaft | 2.0                     | 12.3                     | 5.5                         |
| --maglev --turboshaft            | 1.0                     | 4.9                      | 6.0                         |
| --turbofan --maglev              | 0.9                     | 10.7                     | 9.1                         |
| --maglev                         | 0.7                     | 4.1                      | 6.8                         |

\*% improvement refers to the % improvement over the baseline time (without explicitly enabling any
compiler).
