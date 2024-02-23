# Overview

This is a utility to fetch attestation report and its endorsements.
It works on [Confidential containers on Azure Container Instances](https://learn.microsoft.com/en-gb/azure/container-instances/container-instances-confidential-overview) and also provide an API to provide fake attestation for testing purpose.

## print_snp_json

`print_snp_json` is a utility to print attestation report and its endorsements that can be used for Azure KMS.

Usage:

```bash
# Run the utility
bazel run //scp/cc/azure/attestation:print_snp_json

# Build statically-linked binary
bazel build //scp/cc/azure/attestation:print_snp_json
```
