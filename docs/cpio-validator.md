# CPIO Validator

## Using CPIO Validator for AWS

### Prerequisites for EC2

1. Install Docker CLI tool. Availability can be checked using -

    ```shell
    docker --version
    ```

1. Install Nitro CLI tool. Availability can be checked using -

    ```shell
    nitro-cli --version
    ```

1. Upload the
   [build_and_run_validator_enclave](../scp/cc/public/cpio/validator/build_and_run_validator_enclave)
   script to your EC2 instance.

### Building the validator

1. Build and create a tar archive for the base Docker image.

    ```shell
    builders/tools/bazel-debian build //scp/cc/public/cpio/validator:aws_nitro_enclaves_validator_image.tar
    ```

    Upload it to an EC2 instance and load the image. To load the image -

    ```shell
    DOCKER_IMAGE_TAR="docker image tarball with path"
    docker load -i "${DOCKER_IMAGE_TAR}"
    ```

### Building and Running the Enclave on EC2

1. Make sure the [prerequisites](#prerequisites-for-ec2) are available.
1. Create a text proto ([example](../scp/cc/public/cpio/validator/validator_config.txtpb)) with
   [validator_config](../scp/cc/public/cpio/validator/proto/validator_config.proto) you would like
   to test.
1. Run the validator using the script. Example -

    ```shell
    ./build_and_run_validator_enclave --docker-image-uri bazel/scp/cc/public/cpio/validator:aws_nitro_enclaves_validator_image --validator-conf ./validator_config.txtpb
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
