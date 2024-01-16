# Passing Privacy-Safe Metadata Through Roma

Roma provides functionality to pass metadata associated with invocation request execution to C++
hook functions used within User-Defined Functions (UDFs). This metadata is never serialized, nor
passed into the SAPI Sandbox, allowing for usage of this metadata in these C++ hook functions
without it ever being exposed to untrusted code of the UDF.

Metadata is templated to allow for flexibility regarding the type Roma clients wish to use to pass
said metadata, and defaults to `absl::flat_hash_map<std::string, std::string>`

![Metadata Flow](/images/roma_metadata_flow.png)

-   See more information about Roma [here](/scp/cc/roma/README.md)
-   See examples of how to use this feature in
    [roma/roma_service/metadata_test.cc](/scp/cc/roma/roma_service/metadata_test.cc)

## Implementing Logging

To support logging from UDFs using privacy-safe metadata, the `Config<T>` used to initialize Roma
has been extended to also maintain a custom logging function as a private member.

To set this logging function, `Config<T>` exposes a new function, `Config<T>::SetLoggingFunction()`.

`Config<T>::SetLoggingFunction()` accepts
`absl::AnyInvocable<void(absl::LogSeverity, T, std::string_view) const>`, allowing clients to pass
in any function that accepts:

-   `absl::LogSeverity` - The severity of the log
-   `T` - The metadata associated with invocation request
-   `std::string_view` - The message to be logged

This function will be invoked whenever the provided Roma logging functions are invoked from JS code
(see [here](roma-logging.md) for more details).

Logging is, by default, a no-op unless a logging function is set on the `Config<T>`.
