# Coding Guidelines

These guidelines build upon the [Google C++ Style Guide](http://go/cstyle) and
[C++ Readability](http://go/c-readability).

## Function argument types

-   Use a view class (eg. `std::string_view`) passed by value for transient, read-only access to a
    value. For example, in a getter (lookup) function. An acceptable alternative is to pass a
    constref eg. `const std::string&`.

-   Setter functions, where the function will take control over the object lifetime (eg. storing the
    value into a data structure), use a pass-by-value type eg `std::string`, allowing the caller to
    `std::move` that value if desired. See also
    [Prefer value parameters for small types](http://go/c-readability-advice#value-param).

## Avoid `std::shared_ptr`

Prefer to put objects on the stack. If heap allocation is necessary, use `std::unique_ptr`. When
objects need to share a reference to an object of some type `T`, use one long lived
`std::unique_ptr<T>` and N-1 `T*` instead of N `std::shared_ptr`s.

Alternatives to `std::shared_ptr` when used for purposes other than memory management:

-   [Use `std::optional` for deferred initialization.](http://go/totw/123)
-   [Use `T&` or `T*` for function parameters.](http://go/totw/188)

Exception: when a type inherits from `std::enable_shared_from_this` use of `std::shared_ptr` is
probably necessary.

## Use `reserve` in containers when possible

Containers like `std::vector` and `absl::flat_hash_map` provide a `reserve` method to allocate space
for many additional elements at once. This is especially useful when adding elements to a container
in a for-loop.

## Use absl status macros

Per [TOTW 121: Propagating Status Errors](http://go/totw/121), use the absl status macros available
in the common repo library `//src/util/status_macro:status_macros`. Note: Our macros are prefixed
with `PS_`, therefore `PS_RETURN_IF_ERROR` and `PS_ASSIGN_OR_RETURN`.
