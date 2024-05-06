# Overview

Roma is an open-source C++ library that provides a sandboxed environment for executing untrusted
JavaScript and WASM code. This means that Roma can run code from a third party without risking harm
to the rest of the system.

Roma does this by using two layers of sandboxing:

1. [V8](https://v8.dev/docs): V8 is Google's open source high-performance JavaScript and WebAssembly
   engine, written in C++. It is used in Google Chrome, the open source browser from Google, and in
   Node.js, among others.

    Untrusted code runs in its own sandboxed environment context in one isolate within V8. An
    isolate is a VM instance of the V8 engine with its own memory space. A context is an execution
    environment that allows separate, unrelated, JavaScript code to run in a single instance of V8.
    [V8's security model](https://v8.dev/docs/embed#security-model) is based on the "same-origin
    policy," which prevents code from one website from accessing or modifying the data of another
    website. In V8, an "origin" is defined as a context.

1. [Sandboxed API](https://developers.google.com/code-sandboxing/sandboxed-api): SAPI is a wrapper
   of [google sandbox2](https://developers.google.com/code-sandboxing/sandbox2), which is an
   open-source C++ security sandbox for Linux. In Roma, sandbox2 is used to sandbox the V8 engine to
   provide the second layer of security. Roma sets up
   [Sandbox Policy](https://developers.google.com/code-sandboxing/sandbox2/explained#sandbox_policy),
   which uses seccomp-bpf, a way to restrict system calls, to only allow the system calls that V8
   needs to run. For example, the sandboxee cannot access networking system calls.

In addition to sandboxing, Roma also uses IPC to communicate between the untrusted code execution
and the Roma host process. This further isolates the untrusted code and prevents it from affecting
the rest of the system.

![Roma overview](images/overview.svg 'image_tooltip')

# Roma explained

## Interface

The ROMA interface provides three public static functions for interacting with Roma:

-   **Roma::LoadCodeObj():** This function loads a code object into Roma. Once loaded, the code
    object is cached with its version for future invocations. There are two layers of code caches
    inside Roma:
    [Source Code Cache and Compilation Persistence](#source-code-cache-and-compilation-persistence).
-   **Roma::Execute():** This function executes a single invocation request. The invocation request
    contains only the parameters needed to call the code object, but not the code object itself.
    When the request is sent to the sandbox, the sandbox will retrieve the compiled context or
    source code by its version and run the invocation request with inputs. Refer to
    [Compilation Persistence](#source-code-cache-and-compilation-persistence) for more details.
-   **Roma::BatchExecute():** This function executes a batch of invocation requests. Invocation
    requests in a batch are like invocation requests from `Roma::Execute()`, except that the
    `BatchExecute()` callback function handles the responses of all the requests in the batch. Note:
    the Dispatcher only returns a failure to `Roma::BatchExecute()` when the queue is full before
    the batch starts to enqueue. Once the batch starts to enqueue, the batch blocks until all
    requests in the batch are enqueued.

To learn more about the structure of Roma requests and the API functions that are available, see the
[roma interface](/src/roma/interface/roma.h) directory.

## Data flow

### Roma components

**Dispatcher:** The dispatcher uses an asynchronous executor to schedule tasks for the workers to
execute. For loading requests, the dispatcher sends the request to all workers, who then schedule
the loading request for themselves. For invocation requests, the dispatcher rotates through the
workers in a round-robin fashion and schedules the invocation request to the next worker in the
rotation.

**AsyncExecutor:** The async executor is the main pool of threads for the service. It controls how
many threads are used for Roma workers. The number of threads matches the number of workers, and
each thread has its own queue. One thread is assigned to each Roma worker, and the Roma workers take
tasks from their queues and process them.

**Worker:** Each worker is associated with one sandbox API. The worker executes the tasks in its
thread queue sequentially and synchronously.

**SAPI sandbox:** The SAPI sandbox contains a V8 engine. It passes data between the host process and
the V8 engine.

**V8 engine:** The V8 engine executes the untrusted code.

**UDF handler:** Untrusted code runs in a sandboxed environment, and while it is running, it can
call User Defined Functions (UDF) provided by the trusted party. These functions are executed by the
UDF handler outside of the sandbox, in the Roma host process. The UDF handler is a thread which
listens to UDF invocation requests from the sandbox. There is one UDF handler thread for each
sandbox. Refer to [Function Binding Registration](#user-defined-function-bindings) for more
information about UDF handlers.

![roma dataflow](images/dataflow.svg 'image_tooltip')

## Data Share

There are two layers of data sharing :

-   Layer 1: IPC between the host process and the sandbox.
    -   IPC for request and response: Uses
        [SAPI Pointers](https://developers.google.com/code-sandboxing/sandboxed-api/variables#sapi_pointers)
        and
        [sandbox2::Buffer](https://developers.google.com/code-sandboxing/sandbox2/getting-started/communication#sharing_data_with_buffers).
        This sharing mechanism is configurable, so clients can choose to share the request and
        response using both Pointers and Buffer, or just Buffer.
    -   IPC for UDF input and output: Uses the
        [comms API](https://developers.google.com/code-sandboxing/sandbox2/getting-started/communication#using_the_comms_api)
        to share the protobuf.
-   Layer 2: Data sharing between the SAPI sandbox and the V8 engine is done by converting it to and
    from V8::Value objects. This is because the V8 isolate has its own memory management system.
    Like converting a `std::string` to a `V8::String`, this is essentially copying the data from one
    side to the other.

## Source Code Cache and Compilation Persistence

The sandboxee uses a least recently used (LRU) cache to store code objects and compilation contexts.
This means that the set of most recently used code objects and compilation contexts are kept in the
cache, and the least recently used ones are removed. The size of this cache is configurable.

### Source code cached in Dispatcher

Whenever a new version of a code object is received, the Dispatcher caches it in the LRU cache. This
cache could be used to restore the code objects to a respawn worker after the worker crashed.

### Compilation persistence in Sandboxee

When loading a code object into the sandbox, the sandbox compiles the code and caches the compiled
version. The compilation process differs depending on the type of code that is loaded (just
JavaScript, JavaScript with WASM, just WASM).

#### JavaScript code

V8 uses [V8::Snapshot](https://v8.dev/blog/speeding-up-v8-heap-snapshots) to cache the compiled
version of JavaScript code. When Roma loads JavaScript code, it creates an isolated startup data
with a V8::Snapshot. This startup data can be used to launch a new V8 isolate, in which a
V8::Context with the compiled code can be recreated for each execution.

For JavaScript code loading, an isolate created by the startup data is cached in the Roma
Compilation caches. When executing a call request, you only need to obtain the compilation context
through the version string, enter the isolate and create a new V8::Context to make function calls.

#### JavaScript mixed with WASM code

As V8::Snapshot doesn't support WebAssemably, a compilation
[V8::UnboundScript](https://v8docs.nodesource.com/node-4.8/d4/de9/classv8_1_1_unbound_script.html)
is cached in the isolate, which is used to bind to the new context without needing to recompile.

When loading JavaScript code mixed with WebAssembly code, Roma creates a V8 isolate, compiles the
source code, and exports the compiled script to a global UnboundScript variable stored in the
isolate. The isolate with the unbound script is cached in Roma's compilation cache. When executing a
call request, Roma creates a new V8 context in the isolate and binds the unbound script to the
context. Then, it calls the function with inputs.

#### WASM code

For WASM code, the compilation cache is WASM source code. To execute a call request, the sandbox
enters the cached isolate, even though the isolate does not contain any compiled code. The sandbox
then recompiles the WASM source code from the sandbox compilation cache and uses the compiled code
to call the function.

## User Defined Function Bindings

Roma users can register C++ hook functions, which can then be used by JavaScript. These binding
functions must be registered when Roma is initialized. UDFs can accept a list of strings or a map of
string and bytes as input. This allows users to define their own objects without having to implement
converters for every odd type in Roma.

# Getting Started with Roma

1. To initialize Roma, you need to create a `Config` object. This object defines the number of
   workers, queue size, code object cache size, and resource limit for Roma. For more information on
   how to configure Roma, see the file config.h in the [roma/config](/src/roma/config/config.h)
   directory.

    ```cpp
    // an example of UDF
    void StringInStringOutFunction(proto::FunctionBindingPayload<>& wrapper) {
      wrapper.io_proto.set_output_string(wrapper.io_proto.input_string() + " String from C++");
    }
    ```

    Initialize Roma with its configuration configured.

    ```cpp
    #include "src/roma/config/config.h"
    #include "src/roma/interface/roma.h"

    // create a config for roma initialization
    Config config;

    // Set the number of workers
    config.number_of_workers = 2;

    // Register UDF objects in Config
    auto function_binding_object = make_unique<FunctionBindingObjectV2<>>();
    // Binding the `StringInStringOutFunction`
    function_binding_object->function = StringInStringOutFunction;
    function_binding_object->function_name = "cool_function";
    config.RegisterFunctionBinding(move(function_binding_object));

    // Set the V8 engine resource constraints.
    config.ConfigureJsEngineResourceConstraints(1 /*initial_heap_size_in_mb*/,
                                                15 /*maximum_heap_size_in_mb*/);

    // Init Roma with above config.
    RomaService<> roma_service(std::move(config));
    auto status = roma_service.Init();
    ```

1. Roma needs the code object to be loaded before it can process an invocation request. Roma caches
   the code object after it has been loaded, so that it can be reused for future requests. Roma uses
   a least recently used (LRU) cache to store code objects, but it is important to choose the right
   size for the LRU cache during initialization to ensure that enough versions of code objects are
   cached.

    ```cpp
    auto code_obj = make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_string = "v1";
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "Hello world! " + JSON.stringify(input);
    }
    )JS_CODE";

    status = roma_service.LoadCodeObj(
        move(code_obj), [&](absl::StatusOr<ResponseObject> resp) {
          // define a callback function for response handling.
        });
    ```

1. Roma can execute both single and batch invocation requests. To send an invocation request to
   Roma, you can use the following methods:

    ```cpp
    // Create an invocation request.
    auto execution_obj = make_unique<InvocationStrRequest<>>();
    execution_obj->id = "foo";
    execution_obj->version_string = "v1";
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back("\"Foobar\"");

    status = roma_service.Execute(move(execution_obj),
                          [&](absl::StatusOr<ResponseObject> resp) {
                            // define a callback function for response handling.
                          });

    // Create a batch request and do batch execution.
    auto execution_obj = InvocationStrRequest();
    execution_obj.id = "foo";
    execution_obj.version_string = "v1";
    execution_obj.handler_name = "Handler";
    execution_obj.input.push_back("\"Foobar\"");

    // Here we use the same execution object, but these could be different objects
    // with different inputs, for example.
    vector<InvocationStrRequest<>> batch(5 /*batch size*/, execution_obj);
    status = roma_service.BatchExecute(
        batch, [&](const std::vector<absl::StatusOr<ResponseObject>>& batch_resp) {
          // define a callback function for response handling.
        });
    ```

1. Stop Roma

    ```cpp
    status = roma_service.Stop();
    ```
