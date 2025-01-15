# CPIO Validator

## Using CPIO Validator for AWS

### Prerequisites for EC2

1. Install Docker CLI tool. Availability can be checked with:

    ```shell
    docker --version
    ```

1. Install Nitro CLI tool. Availability can be checked with:

    ```shell
    nitro-cli --version
    ```

    Follow
    [instructions by AWS](https://docs.aws.amazon.com/enclaves/latest/user/nitro-enclave-cli-install.html)
    to install the CLI and devel if not found.

1. Upload the
   [build_and_run_validator_enclave](/src/public/cpio/validator/build_and_run_validator_enclave)
   script to your EC2 instance.

### Building the validator

1. Build and create a tar archive for the base Docker image.

    ```shell
    builders/tools/bazel-debian build //src/public/cpio/validator:aws_nitro_enclaves_validator_tarball
    ```

    Copy it to your to the distribution directory, `dist/aws`:

    ```shell
    builders/tools/bazel-debian run //src/public/cpio/validator:copy_to_dist
    ```

    Upload it to an EC2 instance and load the image. To load the image:

    ```shell
    DOCKER_IMAGE_TAR="docker image tarball with path"
    docker load -i "${DOCKER_IMAGE_TAR}"
    ```

### Building and Running the Enclave on EC2

1. Make sure the [prerequisites](#prerequisites-for-ec2) are available.
1. Create a text proto ([example](/src/public/cpio/validator/validator_config.txtpb)) with
   [validator_config](/src/public/cpio/validator/proto/validator_config.proto) you would like to
   test.
1. Run the validator using the script. Example:

    ```shell
    ./build_and_run_validator_enclave --docker-image-uri bazel/src/public/cpio/validator:aws_nitro_enclaves_validator --validator-conf ./validator_config.txtpb
    ```

#### Sample validation report

```txt
[ SUCCESS ] Connected to outside world.
[ SUCCESS ] Accessed AWS resource.
[ SUCCESS ] GetBlobConfig.CpioValidatorTestBucket.BlobName1
[ SUCCESS ] GetBlobConfig.CpioValidatorTestBucket.BlobName2
[ FAILURE ] GetBlobConfig.CpioValidatorTestBucket.BlobName10 AWS entity not found
[ SUCCESS ] ListBlobsMetadataConfig.CpioValidatorTestBucket
[ FAILURE ] ListBlobsMetadataConfig.CpioValidatorTestBucket1 Internal AWS server error
[ SUCCESS ] GetParameterConfig.CpioValidatorTestParameter
[ FAILURE ] GetParameterConfig.CpioValidatorTestParameter10 Entity not found
[ SUCCESS ] GetTagsByResourceNameConfig
[ SUCCESS ] GetCurrentInstanceResourceNameConfig
```

## Troubleshooting

### Validator does not exit

If the CPIO validator hangs and does not exit for an extended period of time, it is likely that the
VPC environment is incorrectly set up.

### GCP Troubleshooting

[WIP]

### AWS Troubleshooting

#### Re-enable Nitro Service

```shell
sudo systemctl enable --now nitro-enclaves-allocator.service
```

#### E19

[ E19 ] File operation failure. Such error appears when the system fails to perform the requested
file operations, such as opening the EIF file when launching an enclave, or seeking to a specific
offset in the EIF file, or writing to the log file. File:
'/usr/share/nitro_enclaves/blobs//cmdline', failing operation: 'Open'.

Please
[install the latest](https://docs.aws.amazon.com/enclaves/latest/user/nitro-enclave-cli-install.html)
`aws-nitro-enclaves-cli-devel-*` to build a .eif on an EC2.

#### E22

[ E22 ] Insufficient CPUs available in the pool. User provided `cpu-count` is 2, which is more than
the configured CPU pool size.

Please update your `/etc/nitro_enclaves/allocator.yaml` file to 2 or more CPU counts, then
[re-enable the nitro service.](#re-enable-nitro-service)

#### E27

[ E27 ] Insufficient memory available. User provided `memory` is 1024 MB, which is more than the
available hugepage memory.

Please update your `/etc/nitro_enclaves/allocator.yaml` file to have memory higher than 1500 MB,
then [re-enable the nitro service.](#re-enable-nitro-service)

#### E26

[ E26 ] Insufficient memory requested. User provided `memory` is xxx MB, but based on the EIF file
size, the minimum memory should be 800 MB

Please update your `/etc/nitro_enclaves/allocator.yaml` file to have memory higher than 1500 MB,
then [re-enable the nitro service.](#re-enable-nitro-service)
