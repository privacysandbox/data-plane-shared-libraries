# Logging In Roma

Roma provides built-in logging capabilities to help you debug, monitor, and maintain your JavaScript
and WebAssembly (WASM) User-Defined Functions (UDFs). This is particularly useful when you are
debugging or writing complex UDFs.

## Using the roma logging object

To log messages from your JavaScript UDFs in Roma, you will use the global Roma object. This object
exposes the following logging functions:

-   `roma.n_log()`:
    -   Logs an informational message.
    -   Use this for general information about the progress of your UDF execution.
-   `roma.n_warn()`:
    -   Logs a warning message.
    -   Use this for potential problems or unexpected behavior that does not immediately halt UDF
        execution.
-   `roma.n_error()`:
    -   Logs an error message.
    -   Use this for critical issues that prevent your UDF from completing successfully.

## Example

```js
function myUDF(input) {
    // ... UDF logic

    roma.n_log('Processing input: ' + input); // Informational log

    if (someCondition) {
        // Warning log
        roma.n_warn('Potential issue detected in input: ' + input);
    }

    try {
        // ... (code that could potentially throw an error)
    } catch (error) {
        // Error log
        roma.n_error('Error encountered: ' + error.message);
    }
}
```
