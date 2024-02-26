# Usage

Set the variables in `variables.tfvars.json`.

Set the Terraform version to `1.2.3` to stay consistent with K/V and B&A.

```shell
export TERRAFORM_VERSION=1.2.3
```

Initialize Terraform with:

```shell
builders/tools/terraform \
  -chdir=google_internal/terraform/cpio/aws \
  init
```

Validate the Terraform configuration:

```shell
builders/tools/terraform \
  -chdir=google_internal/terraform/cpio/aws \
  validate
```

Apply Terraform with the set variables:

```shell
builders/tools/terraform \
  -chdir=google_internal/terraform/cpio/aws \
  apply \
  --var-file=./variables.tfvars.json
```

After testing, destroy the terraform resources:

```shell
builders/tools/terraform \
  -chdir=google_internal/terraform/cpio/aws \
  destroy \
  --var-file=./variables.tfvars.json
```
