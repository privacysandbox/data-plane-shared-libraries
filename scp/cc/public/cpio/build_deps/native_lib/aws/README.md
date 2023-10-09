# Rule to build an SDK container image and enclave AMI for AWS

## Usage

In your own BUILD file, define a target simlar like:

```python
load("//scp/cc/public/cpio/build_deps/native_lib/aws:aws_sdk_lib_enclave_ami.bzl", "aws_sdk_lib_enclave_ami")
load("@bazel_skylib//rules:common_settings.bzl", "string_flag")

string_flag(
    name = "aws_sdk_lib_enclave_test_ami_name",
    build_setting_default = "aws_sdk_lib_enclave_test",
)

string_flag(
    name = "aws_region_override",
    build_setting_default = "us-west-1",
)

aws_sdk_lib_enclave_ami(
    name = "aws_sdk_lib_enclave_test",
    client_binaries = {"//path/to/test/target:test_binary":[]},
    ...
)
```

The input parameters of the function are:

`name`(required): Name used for the packer target

`inside_tee`(optional): whether this container is running inside tee. Default to True.

`client_binaries`(required): Dict of bazel targets of client binaries with arguments. This is in the
form:

```json
{
    "client_binary_target": ["arg1", "arg2"]
}
```

`additional_files`(optional): Additional files to include in the container root.

`additional_tars`(optional): Additional files include in the container based on their paths within
the tar.

`pkgs_to_install`(optional): Packages to be installed in the container.

`sdk_cmd_override`(optional): "cmd" parameter to use with the container_image. Only used for testing
purposes.

`ami_name`(optional): The name of the generated AMI showing on AWS console. Default to
"aws_sdk_lib_enclave_ami". It is a bazel string label which can be specified in the "bazel run"
command. For the above example, it could be "bazel run
--//path/to/where/the/rule/is/used:aws_sdk_lib_enclave_test_ami_name=aws_sdk_lib_test ...".

`aws_region_override`(optional): Which region the AMI will be uploaded to. It is also a bazel string
label which can be specified in the "bazel run" command.

## Build

Use the following command to build the container and AMI:

```sh
bazel run path/to/where/the/rule/is/used:{name}
```

## Output

1. It will generate the container tar:
   bazel-bin/path/to/where/the/rule/is/used/{name}\_reproducible_container.tar The image name for
   the tar is: bazel/path/to/where/the/rule/is/used:{name}\_container
2. It will generate the AMI and upload to AWS. The AMI ID can be found at the end of the building
   log. The format is like `ami-*`.
3. The hashes can also be found in the building log. Something like:

```unknown
    amazon-ebs.amzn2-ami:   "Measurements": {
    amazon-ebs.amzn2-ami:     "HashAlgorithm": "Sha384 { ... }",
    amazon-ebs.amzn2-ami:     "PCR0": "ecb34d5b39f7ea88c472a30c9cc5c321dc8eaa0b473116219296b33b79c4063557337df10724d256b76a9cef7dfed51a",
    amazon-ebs.amzn2-ami:     "PCR1": "bcdf05fefccaa8e55bf2c8d6dee9e79bbff31e34bf28a99aa19e6b29c37ee80b214a414b7607236edf26fcb78654e63f",
    amazon-ebs.amzn2-ami:     "PCR2": "e965bba95369dbe029cbe57d4f29cbaf7ce72bf9ead7960ae7ebe8c56d4b86c88138c5079f600f368623609358683412"
    amazon-ebs.amzn2-ami:   }
```
