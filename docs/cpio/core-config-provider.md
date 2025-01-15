# ConfigProvider

ConfigProvider is a configuration provider service for SCP C++ library. ConfigProvider is used to
get the values of parameters from the provided configuration .json file. ConfigProvider supports
three types of value: `String`, `Int` and `Bool`.

## Usage

To get the value of a parameter, initlize ConfigProvider with the configuration file, and call the
ConfigProvider's `Get()` function. The inputs of the `Get()` function are the name of the parameter
and the parameter value assigning variable.

### Includes

```cpp
#include "src/core/config_provider/config_provider.h"
```

### Code

```cpp
// The path and name string for configuration .json file.
string full_path = "../test/test_config.json";

// Constructs ConfigProvider with full_path.
google::scp::core::ConfigProvider config(full_path);

// Initializes ConfigProvider.
config.Init();

// Gets the string value for a parameter.
// The data type of out_string must match the value type of "server-ip".
string out_string;
config.Get("server-ip", out_string);

// Gets the int value for a parameter.
int32_t out_int;
config.Get("buffer-length", out_int);

// Gets the bool value for a parameter.
bool out_bool;
config.Get("server-run", out_bool);
```

### Example of configuration .json file

```json
{
    "server-ip": "10.10.10.20",
    "server-run": true,
    "buffer-length": 5000
}
```

## Limitations

-   Only supports .json configuration file.
-   The structure of .json file should be a flat layer.
-   The parameter name and value data type need to be specified to get the value.
