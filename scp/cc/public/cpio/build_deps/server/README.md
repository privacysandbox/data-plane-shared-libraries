# This is a rule to build an SDK container image

## Build container image

The bazel function to build the container is called `sdk_container`

The input parameters of the function are:

`name`(required): Name used for the generated container_image

`sdk_binaries`(required): Bazel target to include in the container with its environment variables.
This is a dict in the form:

```json
{
    "service name": {
        "env1_name": "env1_value",
        "env2_name": "en2_value"
    }
}
```

`platform`(required): platform to run this container on (aws/gcp)

`inside_tee`(required): whether this container is running inside tee.

`client_binaries`(optional): Dict of bazel targets of client binaries with arguments. This is in the
form:

```json
{
    "client_binary_target": ["arg1", "arg2"]
}
```

`is_test_server`(optional): Whether to build the image as test server. Default to false.

`additional_files`(optional): Additional files to include in the container root.

`additional_tars`(optional): Additional files include in the container based on their paths within
the tar.

`pkgs_to_install`(optional): Packages to be installed in the container.

`sdk_cmd_override`(optional): "cmd" parameter to use with the container_image. Only used for testing
purposes.

## Deploy container

You can use `deploy_sdk_container.sh` to load and run the container.

It has two required arguments and one optional argument:

`$1`build_target_path: the path to the bazel build target. If you are using
cc/public/cpio/build_deps/shared/BUILD, then the path would look like:

```sh
cc/public/cpio/build_deps/shared
```

`$2`build_target_name: the name of the build target.

`$3`host_folder_mount (optional): the folder on the host to be used for socket communication.
Default is `/tmp`. This is only used for client binaries running outside tee.

You can load and run the container using:

```sh
./deploy_sdk_container.sh $1 $2 $3
```
