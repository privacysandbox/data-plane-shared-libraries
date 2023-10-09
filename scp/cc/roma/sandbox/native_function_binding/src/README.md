### These are utilities to call exercise a native function binding.

<!-- prettier-ignore-start -->
What is a native function binding?
:    It's a function that can be registered so that it can be called with a given name from the JS
     code, which then calls a user-provided C++ function.

native_function_invoker
:    This is what handles the C++ function invocation from within the JS engine. That is, when a JS
     function that is linked to a C++ function is called in JS code, the invoker is what handler
     sending the call to the user-provided C++ function.

native_function_handler
:    This is what handles calling the user-provided C++. It receives the call from the invoker, and
     returns a response to it after invoking the user-provided function.

native_function_table
:    This is a table that contains a map of function name (as it can be called from JS) to
     std::function. The functions are the user-provided C++ functions that should be called when the
     JS function by the given name is called.
<!-- prettier-ignore-end -->
