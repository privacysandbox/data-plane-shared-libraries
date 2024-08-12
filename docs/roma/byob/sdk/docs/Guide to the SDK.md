# Guide to the SDK

## Background

Roma Bring-Your-Own-Binary (Roma BYOB) uses a single instance of a double-sandboxed Virtual Machine
Monitor (VMM), namely, [gVisor](https://gvisor.dev/).

Inside this sandbox, to ensure per-process isolation, every AdTech-defined function (ie.
user-defined function or UDF) invocation involves the following:

-   [clone](https://man7.org/linux/man-pages/man2/clone.2.html) with appropriate flags.
-   [pivot_root](https://man7.org/linux/man-pages/man2/pivot_root.2.html) for file system isolation.
-   [exec](https://man7.org/linux/man-pages/man2/execve.2.html) the UDF binary.
-   `wait` for execution to complete subject to timeout.
-   Clean up UDF process and `pivot_root`.

gVisor runs as a sandbox container and the binary is executed inside this container within the
clone-pivot_root construct defined above.

See
[Execution Environment Interface](/docs/roma/byob/sdk/docs/udf/Execution%20Environment%20and%20Interface.md)
and [Communication Interface](/docs/roma/byob/sdk/docs/udf/Communication%20Interface.md) for more
details.
