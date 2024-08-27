# Roma Shell CLI Tool

## Overview

The Roma Shell CLI tool provides an interactive environment for loading and executing User Defined
Functions (UDFs). It streamlines local UDF development and debugging, eliminating the need to run
full server binaries or convert JavaScript into server-specific data formats (e.g., K/V server delta
files).

Run With: `builders/tools/bazel-debian run src/roma/tools/v8_cli:roma_shell -- <flags>`

## Key Features

-   REPL Interface: Interactive command-line environment
-   UDF Loading: Ability to load UDFs from files or command line input
-   UDF Execution: Run UDFs with specified arguments
-   Script-based execution: Run shell CLI commands from a specified text file
-   Profiling Support: CPU and memory profiling for JavaScript (V8-based)
-   Arbitrary V8 Flag Support: Pass any arbitrary V8 flag into Roma V8

## Flags

| Flag                 | Description                                                          | Default Value |
| -------------------- | -------------------------------------------------------------------- | ------------- |
| `--enable_profilers` | Enable V8 CPU and Heap Profilers.                                    | `false`       |
| `--file`             | Read a list of Roma CLI tool commands from the specified file.       | `""` (empty)  |
| `--num_workers`      | Number of Roma workers.                                              | `1`           |
| `--timeout`          | Pass custom timeout duration (e.g., "5s", "1m") to execute commands. | `10s`         |
| `--verbose`          | Log all messages from the shell.                                     | `false`       |

## Commands (w/o Profiling)

<!-- prettier-ignore-start -->
<!-- markdownlint-disable line-length -->
| Command   | Description                                   | Usage                                                            | Example                                   |
| --------- | --------------------------------------------- | ---------------------------------------------------------------- | ----------------------------------------- |
| `load`    | Load a UDF from a file or command line input. | `load <version> <path to udf>` (or omit path to read from stdin) | `load v1 src/roma/tools/v8_cli/sample.js` |
| `execute` | Execute a loaded UDF with optional arguments. | `execute <version> <udf name> [args...]`                         | `execute v1 HandleFunc foo bar`           |
| `help`    | Display a list of available commands.         | `help`                                                           | `help`                                    |
| `exit`    | Exit the Roma Shell tool.                     | `exit`                                                           | `exit`                                    |
<!-- markdownlint-enable line-length -->
<!-- prettier-ignore-end -->

## Commands (w Profiling)

<!-- prettier-ignore-start -->
<!-- markdownlint-disable line-length -->
| Command   | Description                                   | Usage                                                            | Example                                                     |
| --------- | --------------------------------------------- | ---------------------------------------------------------------- | ----------------------------------------------------------- |
| `load`    | Load a UDF from a file or command line input. | `load <version> <path to udf>` (or omit path to read from stdin) | `load v1 src/roma/tools/v8_cli/sample.js`                   |
| `execute` | Execute a loaded UDF with optional arguments. | `execute <profiler output file> <version> <udf name> [args...]`  | `execute foo/bar/profiler_output.txt v1 HandleFunc foo bar` |
| `help`    | Display a list of available commands.         | `help`                                                           | `help`                                                      |
| `exit`    | Exit the Roma Shell tool.                     | `exit`                                                           | `exit`                                                      |
<!-- markdownlint-enable line-length -->
<!-- prettier-ignore-end -->
