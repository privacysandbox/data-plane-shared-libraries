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

## Console support

In addition to the global Roma object, the console can also be used to log from UDFs. Support within
Roma has been provided for the following logging functions.

-   `console.log()` - Console counterpart to `roma.n_log()`
-   `console.warn()` - Console counterpart to `roma.n_warn()`
-   `console.error()` - Console counterpart to `roma.n_error()`

## Performance

| Log Size (ch) | Logs Per Second (`roma.()`) | Logs Per Second (`console.()`) |
| ------------- | --------------------------- | ------------------------------ |
| 1             | 5912                        | 23134                          |
| 10            | 5869                        | 22381                          |
| 100           | 5648                        | 23673                          |
| 1000          | 5581                        | 19082                          |
| 10000         | 4973                        | 14347                          |
| 100000        | 2570                        | 2648                           |

Based on these performance stats, console.() is recommended for use over roma.n().

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
