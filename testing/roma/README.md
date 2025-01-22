# Roma Hello World Example

This example demonstrates the basic usage of RomaService to execute a simple JavaScript UDF
(User-Defined Function).

## Overview

The `hello_world_binary.cc` shows how to:

1. Initialize a RomaService
2. Load a JavaScript UDF
3. Execute the UDF
4. Measure execution time
5. Clean up resources

## The JavaScript UDF

The example uses a simple JavaScript function that returns a mock response:

```javascript
function Handler(input) {
    return {
        success: true,
        data: 'CjwKFAoIbWV0YWRhdGESCAoGCgR0ZXN0ChMKB3Byb2R1Y3QSCBIGCgRI4QNDCg8KA3N1bRIIEgYKBFyPNEI=',
    };
}
```

## Building and Running

```bash
builders/tools/bazel-debian build testing/roma:hello_world_binary
bazel-bin/testing/roma/hello_world_binary
```

### Automated Test

To verify both correctness and performance, use the test script:

```bash
testing/roma/hello_world_binary_test
```

The test script will:

1. Build the binary
2. Run the example
3. Verify successful execution
4. Check that execution time is under 10ms
5. Report results

## Expected Output

With manual execution, you should see output similar to:

```bash
Execution duration: 1.2ms
Execution result: {"success":true,"data":"CjwKFAoIbWV0YWRhdGESCAoGCgR0ZXN0ChMKB3Byb2R1Y3QSCBIGCgRI4QNDCg8KA3N1bRIIEgYKBFyPNEI="}
```

With the test script, successful output looks like:

```bash
Building hello_world_binary...
Running hello_world_binary...
SUCCESS: Hello world test passed
Execution time: 1.2ms
```

> **Note**: If execution time is consistently over 10ms, check your compilation flags. The hello
> world example should execute in single-digit milliseconds when properly optimized. The test script
> will fail if execution takes longer than 10ms.
