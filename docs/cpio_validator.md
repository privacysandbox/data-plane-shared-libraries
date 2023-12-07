# CPIO Validator

## Using CPIO Validator for AWS

### Building Proxy

1. Use `scripts/build_proxy` to build proxy. This will create a `dist/aws` directory with proxy
   binaries for different Linux distributions.
1. Look for the `proxy` file for your distribution. Unzip and upload it to your EC2 instance.

### Configuring and building CPIO Validator

1. Take a look at
   [validator_config.proto](./../scp/cc/public/cpio/validator/proto/validator_config.proto).
1. Modify [validator_config.txtpb](./../scp/cc/public/cpio/validator/validator_config.txtpb)
   according to the configuration you will be using on your matchine.
1. Build the docker image -

    ```shell
    builders/tools/bazel-debian build \
      //scp/cc/public/cpio/validator:aws_nitro_enclaves_validator_image.tar
    ```

1. Convert it to an EIF using `builders/tools/convert-docker-to-nitro`. Example -

    ```shell
    builders/tools/convert-docker-to-nitro \
      --docker-image-tar bazel-bin/scp/cc/public/cpio/validator/aws_nitro_enclaves_validator_image.tar \
      --docker-image-tag aws_nitro_enclaves_validator_image \
      --outdir scp/cc/public/cpio/validator/ \
      --eif-name cpio_validator \
      --docker-image-uri bazel/scp/cc/public/cpio/validator
    ```

1. Upload the CPIO validator to your EC2 instance.

### Running the validator

1. Log into your EC2 instance and run proxy.
1. With proxy running, run the validator in debug mode. Example -

    ```shell
    nitro-cli run-enclave --cpu-count 2 --memory 1708 --eif-path cpio_validator.eif --enclave-cid 10 --debug-mode
    ```

1. Check the console for result. Example -

    ```shell
    nitro-cli console --enclave-name cpio_validator
    ```

1. Debug errors (if any).

### Sample validation report

```txt
[ SUCCESS ] GetBlobConfig.CpioValidatorTestBucket.BlobName1
[ SUCCESS ] GetBlobConfig.CpioValidatorTestBucket.BlobName2
[ FAILURE ] GetBlobConfig.CpioValidatorTestBucket.BlobName10 AWS entity not found
[ SUCCESS ] ListBlobsMetadataConfig.CpioValidatorTestBucket
[ FAILURE ] ListBlobsMetadataConfig.CpioValidatorTestBucket1 Internal AWS server error
[ SUCCESS ] Ran all validation tests. For individual statuses, see above.
```
